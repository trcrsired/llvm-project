//===- HerbceptionsLegacyEHFold.cpp - fold legacy throws into herbceptions ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Under -fherbceptions, a legacy C++ throw (__cxa_throw / _CxxThrowException)
// whose unwind edge provably reaches only the compiler-generated
// legacy-to-std::error conversion site can be folded into a direct conversion
// call: the libherbceptions __cxa_error_code_*_exception_ptr_direct entry
// points produce the same error code the catch-site conversion would, without
// ever raising the exception. This removes the unwind edge, the EH dispatch
// (catchswitch/landingpad) hop, and the funclet crossing.
//
// The fold is only legal when conversion is the *only* possible outcome of
// the throw's unwind edge:
//
//  * The unwind destination must be a catchswitch whose sole handler is the
//    catch-all conversion pad (MSVC/Wasm funclet model), or a landingpad with
//    only `catch ptr null` clauses that flows to the conversion calls
//    (Itanium model). Typed handlers sharing the dispatch could claim the
//    exception first, so their presence disables the fold.
//
//  * No intervening cleanup may be skipped: the invoke must unwind directly
//    to the conversion dispatch.
//
//  * Nothing may observe the in-flight exception. This holds because the
//    conversion site is the only destination and it only inspects the
//    exception through the conversion ABI calls. Real catches must not be
//    folded: catch(T) clauses observe the exception object and
//    std::current_exception(), which the folded path does not provide.
//
// The exception object's construction (which can itself throw, e.g. a
// std::runtime_error string copy failing allocation) keeps its normal invoke;
// only the final __cxa_throw/_CxxThrowException is folded.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar/HerbceptionsLegacyEHFold.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/EHPersonalities.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsWebAssembly.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"
#include <algorithm>
#include <utility>

using namespace llvm;

#define DEBUG_TYPE "herbceptions-legacy-eh-fold"

namespace {

constexpr StringLiteral MsvcDomainFnName =
    "__cxa_error_domain_msvc_exception_ptr";
constexpr StringLiteral MsvcCodeFnName =
    "__cxa_error_code_msvc_exception_ptr";
constexpr StringLiteral MsvcDirectFnName =
    "__cxa_error_code_msvc_exception_ptr_direct";
constexpr StringLiteral ItaniumDomainFnName =
    "__cxa_error_domain_itanium_exception_ptr";
constexpr StringLiteral ItaniumCodeFnName =
    "__cxa_error_code_itanium_exception_ptr";
constexpr StringLiteral ItaniumDirectFnName =
    "__cxa_error_code_itanium_exception_ptr_direct";

enum class ThrowABI { Itanium, MSVC };

/// A validated conversion dispatch reached by a foldable throw edge.
struct ConvSite {
  /// The conversion calls; their results are what downstream code consumes.
  CallInst *DomainCall = nullptr;
  CallInst *CodeCall = nullptr;
  /// The block folded edges branch to. For the funclet model this is the
  /// (possibly trampolined) catchret continuation. For the landingpad model
  /// this is produced by splitting the conversion block after the calls.
  BasicBlock *ContBB = nullptr;
  /// Landingpad model only: the conversion block and the instruction to
  /// split it before.
  BasicBlock *ConvBB = nullptr;
  Instruction *SplitBefore = nullptr;
  /// Instructions making up the conversion dispatch (everything a folded
  /// edge bypasses); only DomainCall and CodeCall may have uses outside.
  SmallPtrSet<Instruction *, 8> ConvInsts;
};

static bool isNullConstant(Value *V) {
  auto *C = dyn_cast<Constant>(V);
  return C && C->isNullValue();
}

/// True if I is a call to the named function; records it in Out.
static bool isNamedCall(Instruction &I, StringRef Name, CallInst *&Out) {
  auto *CI = dyn_cast<CallInst>(&I);
  if (!CI)
    return false;
  Function *F = CI->getCalledFunction();
  if (!F || F->getName() != Name)
    return false;
  Out = CI;
  return true;
}

/// Instructions allowed on the conversion path: EH glue, the conversion
/// calls, and pure plumbing feeding the code call. Anything else means the
/// dispatch does real work the fold would skip.
static bool isAllowedConvInst(Instruction &I, ThrowABI ABI,
                              CallInst *&DomainCall, CallInst *&CodeCall,
                              SmallPtrSetImpl<Instruction *> &ConvInsts) {
  if (isa<CallInst>(I)) {
    StringRef DomName =
        ABI == ThrowABI::MSVC ? MsvcDomainFnName : ItaniumDomainFnName;
    StringRef CodeName =
        ABI == ThrowABI::MSVC ? MsvcCodeFnName : ItaniumCodeFnName;
    CallInst *Match = nullptr;
    if (isNamedCall(I, DomName, Match)) {
      if (DomainCall)
        return false;
      DomainCall = Match;
      ConvInsts.insert(Match);
      return true;
    }
    if (isNamedCall(I, CodeName, Match)) {
      if (CodeCall)
        return false;
      CodeCall = Match;
      ConvInsts.insert(Match);
      return true;
    }
    // Wasm catchpads extract the in-flight exception through intrinsics.
    if (auto *II = dyn_cast<IntrinsicInst>(&I))
      if (II->getIntrinsicID() == Intrinsic::wasm_get_exception ||
          II->getIntrinsicID() == Intrinsic::wasm_get_ehselector) {
        ConvInsts.insert(&I);
        return true;
      }
    return false;
  }
  if (isa<CatchPadInst, CatchSwitchInst, LandingPadInst>(I) ||
      isa<ExtractValueInst, PHINode, CatchReturnInst>(I) ||
      I.isDebugOrPseudoInst() || I.isLifetimeStartOrEnd()) {
    ConvInsts.insert(&I);
    return true;
  }
  return false;
}

/// Follow the funclet exit chain from the conversion pad: catchret to the
/// continuation, allowing only empty cleanup trampolines in between.
static BasicBlock *
followFuncletExit(BasicBlock *PadBB, SmallPtrSetImpl<Instruction *> &ConvInsts,
                  SmallPtrSetImpl<BasicBlock *> &Visited) {
  auto *CRI = dyn_cast<CatchReturnInst>(PadBB->getTerminator());
  if (!CRI)
    return nullptr;
  ConvInsts.insert(CRI);
  BasicBlock *Cont = CRI->getSuccessor();
  while (true) {
    if (!Visited.insert(Cont).second)
      return nullptr;
    auto It = Cont->getFirstNonPHIIt();
    if (It == Cont->end() || !isa<CleanupPadInst>(&*It))
      return Cont;
    // An intermediate cleanup funclet may only contain the funclet glue: it
    // exists solely to exit the funclet, and the fold bypasses it.
    auto *CLRI = dyn_cast<CleanupReturnInst>(Cont->getTerminator());
    if (!CLRI || CLRI->unwindsToCaller())
      return nullptr;
    for (Instruction &I : *Cont)
      if (!isa<CleanupPadInst, CleanupReturnInst>(I) &&
          !I.isDebugOrPseudoInst())
        return nullptr;
    for (Instruction &I : *Cont)
      ConvInsts.insert(&I);
    Cont = CLRI->getUnwindDest();
  }
}

/// The code call's argument, when it has one, must be computed inside the
/// conversion dispatch (e.g. the landingpad/wasm.get.exception exn value);
/// anything else means the value being converted is not the exception this
/// edge delivers.
static bool codeCallArgIsConvLocal(const ConvSite &Site) {
  if (Site.CodeCall->arg_empty())
    return true;
  auto *I = dyn_cast<Instruction>(Site.CodeCall->getArgOperand(0));
  return I && Site.ConvInsts.contains(I);
}

/// Funclet model (MSVC and wasm): the invoke unwinds to a catchswitch whose
/// single handler is a catch-all pad holding the conversion calls.
static bool matchFuncletConversion(InvokeInst &II, ThrowABI ABI,
                                   ConvSite &Site) {
  BasicBlock *UnwindDest = II.getUnwindDest();
  auto *CSI =
      dyn_cast_or_null<CatchSwitchInst>(&*UnwindDest->getFirstNonPHIIt());
  if (!CSI || CSI->getNumHandlers() != 1)
    return false;

  BasicBlock *PadBB = *CSI->handler_begin();
  auto *CPI = dyn_cast_or_null<CatchPadInst>(&*PadBB->getFirstNonPHIIt());
  if (!CPI || CPI->getCatchSwitch() != CSI)
    return false;
  // Only a catch-all pad converts every exception; a typed pad would only
  // run for matching types, and anything it failed to match would keep
  // unwinding instead of converting.
  for (Use &Arg : CPI->arg_operands())
    if (!isNullConstant(Arg.get()))
      return false;

  for (Instruction &I : *PadBB)
    if (!isAllowedConvInst(I, ABI, Site.DomainCall, Site.CodeCall,
                           Site.ConvInsts))
      return false;
  if (!Site.DomainCall || !Site.CodeCall)
    return false;

  SmallPtrSet<BasicBlock *, 8> Visited{PadBB};
  Site.ContBB = followFuncletExit(PadBB, Site.ConvInsts, Visited);
  if (!Site.ContBB || Site.ContBB->isEHPad() || !codeCallArgIsConvLocal(Site))
    return false;
  return true;
}

/// Itanium model: the invoke unwinds to a catch-all-only landingpad that
/// flows to a (possibly shared) block holding the conversion calls.
static bool matchLandingpadConversion(InvokeInst &II, ConvSite &Site) {
  BasicBlock *UnwindDest = II.getUnwindDest();
  auto *LPI =
      dyn_cast_or_null<LandingPadInst>(&*UnwindDest->getFirstNonPHIIt());
  if (!LPI || LPI->getNumClauses() == 0)
    return false;
  for (unsigned I = 0, E = LPI->getNumClauses(); I != E; ++I)
    if (!LPI->isCatch(I) || !isNullConstant(LPI->getClause(I)))
      return false;

  // Walk the single-successor chain to the conversion block. Intermediate
  // blocks may only contain pure plumbing; a conditional edge means the
  // exception can go somewhere other than conversion.
  BasicBlock *BB = UnwindDest;
  SmallPtrSet<BasicBlock *, 8> Visited;
  for (unsigned Depth = 0; Depth != 16; ++Depth) {
    if (!Visited.insert(BB).second)
      return false;

    // Does this block hold the conversion call pair?
    CallInst *DomainCall = nullptr, *CodeCall = nullptr;
    for (Instruction &I : *BB) {
      CallInst *M = nullptr;
      if (isNamedCall(I, ItaniumDomainFnName, M))
        DomainCall = M;
      else if (isNamedCall(I, ItaniumCodeFnName, M))
        CodeCall = M;
    }

    if (DomainCall && CodeCall) {
      // Split point is just past the later call; everything before it is
      // bypassed by folded edges and must be conversion plumbing. The two
      // calls themselves are skipped in the check (they seeded the match).
      Instruction *After = DomainCall->comesBefore(CodeCall)
                               ? CodeCall->getNextNode()
                               : DomainCall->getNextNode();
      if (!After)
        return false;
      Site.ConvInsts.insert(DomainCall);
      Site.ConvInsts.insert(CodeCall);
      CallInst *DupD = DomainCall, *DupC = CodeCall;
      for (Instruction &I : *BB) {
        if (&I == After)
          break;
        if (&I == DomainCall || &I == CodeCall)
          continue;
        if (!isAllowedConvInst(I, ThrowABI::Itanium, DupD, DupC,
                               Site.ConvInsts))
          return false;
      }
      Site.DomainCall = DomainCall;
      Site.CodeCall = CodeCall;
      Site.ConvBB = BB;
      Site.SplitBefore = After;
      if (!codeCallArgIsConvLocal(Site))
        return false;
      return true;
    }

    // Intermediate block: everything must be pure plumbing with a single
    // unconditional exit.
    CallInst *IgnoredD = nullptr, *IgnoredC = nullptr;
    for (Instruction &I : *BB) {
      if (isa<UncondBrInst>(I))
        continue;
      if (!isAllowedConvInst(I, ThrowABI::Itanium, IgnoredD, IgnoredC,
                             Site.ConvInsts))
        return false;
    }
    auto *BI = dyn_cast<UncondBrInst>(BB->getTerminator());
    if (!BI)
      return false;
    BB = BI->getSuccessor(0);
  }
  return false;
}

/// Folded edges bypass everything in the conversion dispatch except the two
/// call results, so nothing else defined there may be used downstream.
static bool crossingValuesFoldable(const ConvSite &Site) {
  for (Instruction *I : Site.ConvInsts) {
    if (I == Site.DomainCall || I == Site.CodeCall)
      continue;
    for (User *U : I->users())
      if (auto *UI = dyn_cast<Instruction>(U))
        if (!Site.ConvInsts.contains(UI))
          return false;
  }
  return true;
}

static FunctionCallee getDirectFn(Module &M, ThrowABI ABI, Type *CodeRetTy,
                                  LLVMContext &Ctx) {
  Type *PtrTy = PointerType::getUnqual(Ctx);
  bool IsMSVC = ABI == ThrowABI::MSVC;
  FunctionType *FTy =
      IsMSVC ? FunctionType::get(CodeRetTy, {PtrTy, PtrTy}, false)
             : FunctionType::get(CodeRetTy, {PtrTy, PtrTy, PtrTy}, false);
  FunctionCallee FC =
      M.getOrInsertFunction(IsMSVC ? MsvcDirectFnName : ItaniumDirectFnName,
                            FTy);
  auto *F = cast<Function>(FC.getCallee());
  // libherbceptions is a DLL on Windows targets.
  if (M.getTargetTriple().isOSWindows() &&
      F->getDLLStorageClass() == GlobalValue::DefaultStorageClass)
    F->setDLLStorageClass(GlobalValue::DLLImportStorageClass);
  if (!F->doesNotThrow())
    F->setDoesNotThrow();
  return FC;
}

} // end anonymous namespace

PreservedAnalyses
HerbceptionsLegacyEHFoldPass::run(Function &F, FunctionAnalysisManager &) {
  if (F.isDeclaration() || !F.hasPersonalityFn())
    return PreservedAnalyses::all();

  SmallVector<InvokeInst *, 4> Throws;
  for (BasicBlock &BB : F) {
    auto *II = dyn_cast<InvokeInst>(BB.getTerminator());
    if (!II)
      continue;
    Function *Callee = II->getCalledFunction();
    if (!Callee)
      continue;
    if (Callee->getName() == "__cxa_throw" ||
        Callee->getName() == "_CxxThrowException")
      Throws.push_back(II);
  }
  if (Throws.empty())
    return PreservedAnalyses::all();

  EHPersonality Pers = classifyEHPersonality(F.getPersonalityFn());
  // Scoped-EH personalities (MSVC funclets and wasm) both use the
  // catchswitch/catchpad/catchret IR shape.
  bool IsFunclet = isScopedEHPersonality(Pers);
  DenseMap<BasicBlock *, ColorVector> Colors;
  if (IsFunclet)
    Colors = colorEHFunclets(F);

  // Validate every site first; folding mutates the CFG.
  struct FoldableSite {
    InvokeInst *II;
    ThrowABI ABI;
    ConvSite Site;
  };
  SmallVector<FoldableSite, 4> Worklist;
  for (InvokeInst *II : Throws) {
    ThrowABI ABI = II->getCalledFunction()->getName() == "_CxxThrowException"
                       ? ThrowABI::MSVC
                       : ThrowABI::Itanium;
    // A MSVC rethrow (_CxxThrowException(nullptr, nullptr)) is not a fresh
    // throw and must not be folded.
    if (ABI == ThrowABI::MSVC &&
        (isNullConstant(II->getArgOperand(0)) ||
         isNullConstant(II->getArgOperand(1))))
      continue;
    ConvSite Site;
    bool Ok = IsFunclet ? matchFuncletConversion(*II, ABI, Site)
                        : matchLandingpadConversion(*II, Site);
    if (!Ok || !crossingValuesFoldable(Site))
      continue;
    Worklist.push_back({II, ABI, std::move(Site)});
  }
  if (Worklist.empty())
    return PreservedAnalyses::all();

  // Funclet legality: the branch from the site to the continuation must stay
  // within the same funclet (or at function level).
  for (auto It = Worklist.begin(); It != Worklist.end();) {
    if (!IsFunclet) {
      ++It;
      continue;
    }
    const ColorVector &SiteColors = Colors.lookup(It->II->getParent());
    const ColorVector &ContColors = Colors.lookup(It->Site.ContBB);
    bool Same = SiteColors.size() == ContColors.size() &&
                std::is_permutation(SiteColors.begin(), SiteColors.end(),
                                    ContColors.begin());
    // With a multi-color (shared) block the correct funclet token is
    // ambiguous; only fold when both sides are function-level or inside one
    // identical funclet and the invoke carries the token.
    if (!Same || (SiteColors.size() > 1 &&
                  !It->II->getOperandBundle(LLVMContext::OB_funclet)))
      It = Worklist.erase(It);
    else
      ++It;
  }
  if (Worklist.empty())
    return PreservedAnalyses::all();

  // Landingpad model: split each conversion block after its calls once.
  DenseMap<BasicBlock *, BasicBlock *> ConvBBToCont;
  for (FoldableSite &WS : Worklist) {
    if (IsFunclet)
      continue;
    ConvSite &Site = WS.Site;
    BasicBlock *&ContBB = ConvBBToCont[Site.ConvBB];
    if (!ContBB)
      ContBB = SplitBlock(Site.ConvBB, Site.SplitBefore->getIterator());
    ContBB->setName("herb.conv.cont");
    Site.ContBB = ContBB;
  }

  // Set up merges before any site edge exists, so every existing
  // predecessor of ContBB correctly contributes the conversion result.
  // Downstream uses of the conversion results come in two shapes:
  //   - phi incoming uses on edges *into* ContBB: the phi keeps the
  //     original call result for the conversion edges and gains one
  //     incoming per folded site.
  //   - all other uses: replaced by a merge phi at the top of ContBB that
  //     selects between the conversion result (old preds) and the direct
  //     results (folded sites).
  struct Merge {
    PHINode *PN = nullptr;
    SmallVector<PHINode *, 2> EdgePhis;
  };
  DenseMap<CallInst *, Merge> Merges;
  for (FoldableSite &WS : Worklist) {
    ConvSite &Site = WS.Site;
    for (CallInst *OrigCall : {Site.DomainCall, Site.CodeCall}) {
      auto [It, Inserted] = Merges.try_emplace(OrigCall);
      if (!Inserted)
        continue;
      Merge &M = It->second;
      bool NeedPN = false;
      for (Use &U : OrigCall->uses()) {
        auto *UI = dyn_cast<Instruction>(U.getUser());
        if (!UI || Site.ConvInsts.contains(UI))
          continue;
        auto *UserPN = dyn_cast<PHINode>(UI);
        if (UserPN && UserPN->getParent() == Site.ContBB) {
          if (!is_contained(M.EdgePhis, UserPN))
            M.EdgePhis.push_back(UserPN);
          continue;
        }
        NeedPN = true;
      }
      if (!NeedPN)
        continue;
      M.PN = PHINode::Create(OrigCall->getType(), 0,
                             OrigCall->getName() + ".fold",
                             Site.ContBB->getFirstNonPHIIt());
      for (BasicBlock *Pred : predecessors(Site.ContBB))
        M.PN->addIncoming(OrigCall, Pred);
      OrigCall->replaceUsesWithIf(M.PN, [&](Use &U) {
        auto *UI = dyn_cast<Instruction>(U.getUser());
        if (!UI || UI == M.PN || Site.ConvInsts.contains(UI))
          return false;
        if (auto *UserPN = dyn_cast<PHINode>(UI))
          if (UserPN->getParent() == Site.ContBB)
            return false;
        return true;
      });
    }
  }

  for (FoldableSite &WS : Worklist) {
    InvokeInst *II = WS.II;
    ConvSite &Site = WS.Site;
    BasicBlock *SiteBB = II->getParent();
    BasicBlock *ContBB = Site.ContBB;

    IRBuilder<> B(II);
    B.SetCurrentDebugLocation(II->getDebugLoc());
    SmallVector<OperandBundleDef, 1> Bundles;
    II->getOperandBundlesAsDefs(Bundles);

    CallInst *Dom = B.CreateCall(
        Site.DomainCall->getCalledFunction()->getFunctionType(),
        Site.DomainCall->getCalledFunction(), {}, Bundles, "herb.dom");

    SmallVector<Value *, 3> Args;
    for (Use &U : II->args())
      Args.push_back(U.get());
    FunctionCallee Direct =
        getDirectFn(*F.getParent(), WS.ABI, Site.CodeCall->getType(),
                    F.getContext());
    CallInst *Code = B.CreateCall(Direct, Args, Bundles, "herb.code");

    B.CreateBr(ContBB);
    BasicBlock *NormalDest = II->getNormalDest();
    II->eraseFromParent();
    NormalDest->removePredecessor(SiteBB);

    for (auto [OrigCall, NewVal] :
         {std::pair<CallInst *, Value *>(Site.DomainCall, Dom),
          std::pair<CallInst *, Value *>(Site.CodeCall, Code)}) {
      Merge &M = Merges[OrigCall];
      if (M.PN)
        M.PN->addIncoming(NewVal, SiteBB);
      for (PHINode *EdgePhi : M.EdgePhis)
        EdgePhi->addIncoming(NewVal, SiteBB);
    }
  }

  // Folded edges may have left the dispatch and the invoke's normal
  // destination unreachable.
  removeUnreachableBlocks(F);
  return PreservedAnalyses::none();
}
