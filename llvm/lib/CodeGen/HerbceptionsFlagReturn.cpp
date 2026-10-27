//===- HerbceptionsFlagReturn.cpp - Prepare return flags ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A 'throws' function returns {payload, i1}, where the trailing i1 is the
// discriminant: it says whether the return registers hold a value or an
// error. The backend lowers it to a condition-code flag where it can (CF on
// x86, NZCV.C on AArch64, ...).
//
// SimplifyCFG merges all returns into one block, so the discriminant usually
// reaches the return through a phi or is recomputed from a value that has to
// stay live across a called 'throws' function. Instruction selection then
// materializes it in a register only to turn it back into a flag at the
// return, and may have to preserve the inputs of that computation.
//
// This pass duplicates a small return block into its predecessors and, where
// the branch leading to the return already implies the discriminant, replaces
// it with a constant there. The backend then sets the flag directly (stc/clc
// on x86) or folds it into the return sequence (cmp on AArch64) instead of
// materializing the value, and the computation that fed it is dead.
//
// The transform only rewrites how the return value is built: the signature,
// the calling convention and the ABI do not change.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

using namespace llvm;

#define DEBUG_TYPE "herbceptions-flag-return"

namespace {

class HerbceptionsFlagReturn : public FunctionPass {
public:
  static char ID;

  HerbceptionsFlagReturn() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<DominatorTreeWrapperPass>();
  }

  StringRef getPassName() const override {
    return "Herbceptions Return Flag Folding";
  }
};

} // end anonymous namespace

char HerbceptionsFlagReturn::ID = 0;

INITIALIZE_PASS(HerbceptionsFlagReturn, DEBUG_TYPE,
                "Fold herbceptions return flags to constants", false, false)

FunctionPass *llvm::createHerbceptionsFlagReturnPass() {
  return new HerbceptionsFlagReturn();
}

/// The use of the flag that \p RI returns, if it is set by an insertvalue.
static Use *getFlagUse(ReturnInst *RI) {
  auto *STy = dyn_cast_or_null<StructType>(
      RI->getReturnValue() ? RI->getReturnValue()->getType() : nullptr);
  if (!STy || STy->getNumElements() == 0 ||
      !STy->getElementType(STy->getNumElements() - 1)->isIntegerTy(1))
    return nullptr;
  unsigned FlagIdx = STy->getNumElements() - 1;
  Value *V = RI->getReturnValue();
  while (auto *IV = dyn_cast<InsertValueInst>(V)) {
    if (IV->getNumIndices() == 1 && IV->getIndices()[0] == FlagIdx)
      return &IV->getOperandUse(
          InsertValueInst::getInsertedValueOperandIndex());
    V = IV->getAggregateOperand();
  }
  return nullptr;
}

/// Whether the return block \p BB is worth duplicating into its predecessors.
static bool shouldDuplicate(BasicBlock *BB, ReturnInst *RI) {
  if (pred_empty(BB) || BB->hasNPredecessorsOrMore(9) || BB->hasAddressTaken())
    return false;
  // The flag must be set in the block, possibly to a phi, or the value must
  // be a phi. A flag that is already a constant needs no work.
  Use *Flag = getFlagUse(RI);
  auto *PN = dyn_cast_or_null<PHINode>(RI->getReturnValue());
  if (Flag ? isa<Constant>(Flag->get()) : !(PN && PN->getParent() == BB))
    return false;
  // Only the construction of the return value, plus whatever computes the
  // discriminant (late passes often recompute the branch condition here
  // rather than keep it live across the edge). Duplicating is safe: each
  // copy runs exactly on the edge that reached the block, so the same
  // instructions execute on the same paths, just from more than one place.
  unsigned NumInsts = 0;
  for (Instruction &I : *BB) {
    if (isa<PHINode>(I) || isa<DbgInfoIntrinsic>(I))
      continue;
    if (I.isTerminator()) {
      if (!isa<ReturnInst>(I))
        return false;
      continue;
    }
    if (++NumInsts > 8)
      return false;
  }
  for (BasicBlock *Pred : predecessors(BB))
    if (!isa<UncondBrInst, CondBrInst>(Pred->getTerminator()))
      return false;
  return true;
}

/// The value of \p Cond at \p I if it is implied by a dominating branch.
static std::optional<bool> getImpliedValue(Value *Cond, Instruction *I,
                                           const DataLayout &DL) {
  // CodeGenPrepare freezes the conditions of branches that it creates from
  // selects.
  BasicBlock *BB = I->getParent();
  BasicBlock *Pred = BB->getSinglePredecessor();
  auto *Br = Pred ? dyn_cast<CondBrInst>(Pred->getTerminator()) : nullptr;
  if (Br && Br->getSuccessor(0) != Br->getSuccessor(1)) {
    Value *BrCond = Br->getCondition();
    if (auto *Fr = dyn_cast<FreezeInst>(BrCond))
      BrCond = Fr->getOperand(0);
    if (BrCond == Cond)
      return Br->getSuccessor(0) == BB;
  }
  return isImpliedByDomCondition(Cond, I, DL);
}

/// Copy the return block \p BB into \p Pred, which ends in an unconditional
/// branch to it. Returns the new return instruction.
static ReturnInst *duplicateReturnInto(BasicBlock *BB, BasicBlock *Pred,
                                       DominatorTree *DT) {
  ValueToValueMapTy VMap;
  for (PHINode &PN : BB->phis())
    VMap[&PN] = PN.getIncomingValueForBlock(Pred);
  Instruction *Br = Pred->getTerminator();
  ReturnInst *NewRet = nullptr;
  for (Instruction &I : *BB) {
    if (isa<PHINode>(I))
      continue;
    Instruction *C = I.clone();
    C->insertBefore(Br->getIterator());
    RemapInstruction(C, VMap, RF_NoModuleLevelChanges | RF_IgnoreMissingLocals);
    VMap[&I] = C;
    if (auto *RI = dyn_cast<ReturnInst>(C))
      NewRet = RI;
  }
  BB->removePredecessor(Pred);
  Br->eraseFromParent();
  DT->deleteEdge(Pred, BB);
  return NewRet;
}

bool HerbceptionsFlagReturn::runOnFunction(Function &F) {
  // Only 'throws' functions return a trailing discriminant. The transform
  // grows the function by duplicating a small return block, so leave
  // functions that are compiled for size alone.
  if (skipFunction(F) || !F.hasFnAttribute(Attribute::Throws) ||
      F.hasMinSize())
    return false;

  const DataLayout &DL = F.getDataLayout();
  DominatorTree *DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();

  SmallVector<BasicBlock *, 4> Returns;
  for (BasicBlock &BB : F)
    if (auto *RI = dyn_cast<ReturnInst>(BB.getTerminator()))
      if (shouldDuplicate(&BB, RI))
        Returns.push_back(&BB);

  bool Changed = false;
  for (BasicBlock *BB : Returns) {
    SmallVector<BasicBlock *, 4> Preds(predecessors(BB));
    SmallVector<ReturnInst *, 4> NewRets;
    for (BasicBlock *Pred : Preds) {
      // Give each edge a block of its own that ends in an unconditional
      // branch to BB, into which the return is folded.
      BasicBlock *From = Pred;
      if (!isa<UncondBrInst>(Pred->getTerminator()))
        From = SplitEdge(Pred, BB, DT);
      NewRets.push_back(duplicateReturnInto(BB, From, DT));
    }
    // Where the flag is implied by the branch leading to the return, use the
    // constant. The insertvalue that sets the flag may be shared with other
    // paths, so set the constant in a new one right before the return.
    for (ReturnInst *NewRI : NewRets) {
      Use *Flag = getFlagUse(NewRI);
      if (!Flag || isa<Constant>(Flag->get()))
        continue;
      if (std::optional<bool> Known = getImpliedValue(Flag->get(), NewRI, DL)) {
        Value *RetVal = NewRI->getReturnValue();
        unsigned FlagIdx =
            cast<StructType>(RetVal->getType())->getNumElements() - 1;
        NewRI->setOperand(
            0, InsertValueInst::Create(
                   RetVal, ConstantInt::getBool(F.getContext(), *Known),
                   FlagIdx, "", NewRI->getIterator()));
      }
    }
    if (pred_empty(BB)) {
      // Deleting the last edge usually removed it from the tree already.
      if (DT->getNode(BB))
        DT->eraseNode(BB);
      DeleteDeadBlock(BB);
    }
    Changed = true;
  }
  return Changed;
}
