//===-- X86HerbceptionFold.cpp - Fold the throws discriminant test --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A herbception (throws) function returns its failure discriminant in the carry
// flag, and clang materialises it immediately after the call:
//
//   %disc = setb implicit EFLAGS
//   ...
//   testb $1, %disc
//   je/jne ...
//
// When nothing redefines the flags in between, the test and the branch collapse
// into a single branch on live CF, removing two instructions from every throws
// call site.
//
// This runs after prolog/epilog insertion, because that is when the question
// the fold depends on first has an answer. What sits between the call and the
// branch at that point may include the call's stack adjustment, which is
// carried as an ADJCALLSTACKDOWN/UP pseudo from instruction selection until
// prolog/epilog insertion either merges it away or materialises it as a real
// add/sub. Such an add redefines the flags the fold would come to depend on, so
// deciding the fold before prolog/epilog insertion means deciding on a machine
// that is still going to change.
//
// That is not a hypothetical: doing the fold earlier is how i686 used to end up
// with
//
//   calll _foo
//   addl  $4, %esp     <- CF is now the cleanup's carry-out, always clear
//   jae   .Lcont       <- folded onto that carry, so the error path is dead
//
// and 64-bit Windows, where the same pseudo happens to be merged away, kept a
// correct but accidental fold. Deciding after prolog/epilog insertion gets
// both right without having to model what frame lowering will do.
//
//===----------------------------------------------------------------------===//

#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "x86-herbception-fold"

STATISTIC(NumFolded, "Number of throws discriminant tests folded onto CF");

namespace {

class X86HerbceptionFold : public MachineFunctionPass {
public:
  static char ID;

  X86HerbceptionFold() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};

} // end anonymous namespace

char X86HerbceptionFold::ID = 0;

INITIALIZE_PASS(X86HerbceptionFold, DEBUG_TYPE,
                "Fold throws discriminant tests onto the carry flag", false,
                false)

FunctionPass *llvm::createX86HerbceptionFoldPass() {
  return new X86HerbceptionFold();
}

/// Whether the value written by \p SetB is read anywhere before it is
/// overwritten, following the CFG. A per-block visited set makes this
/// conservative: a block reached along several paths is only examined once, so
/// a path that would have killed the value can still be treated as live. That
/// only costs the fold an instruction it could have removed.
static bool isDefLive(MachineInstr &SetB, Register Reg,
                      const TargetRegisterInfo *TRI) {
  MachineBasicBlock *DefMBB = SetB.getParent();
  SmallVector<MachineBasicBlock *, 8> Work;
  SmallPtrSet<MachineBasicBlock *, 8> Visited;

  // The defining block is treated as visited so a back edge cannot re-examine
  // it from the top, where an unrelated read of the same register sits.
  Visited.insert(DefMBB);

  auto ScanInto = [&](MachineBasicBlock &B, MachineBasicBlock::iterator From) {
    for (auto It = From; It != B.end(); ++It) {
      MachineInstr &MI = *It;
      if (MI.readsRegister(Reg, TRI))
        return true;
      // Overwritten on this path, so the setb's value cannot reach further.
      if (MI.definesRegister(Reg, TRI))
        return false;
    }
    for (MachineBasicBlock *Succ : B.successors())
      if (Visited.insert(Succ).second)
        Work.push_back(Succ);
    return false;
  };

  if (ScanInto(*DefMBB, std::next(SetB.getIterator())))
    return true;

  while (!Work.empty()) {
    MachineBasicBlock *B = Work.pop_back_val();
    if (ScanInto(*B, B->begin()))
      return true;
  }

  return false;
}

/// Fold `%disc = setb; testb $1, %disc; j<e|ne>` into a branch on live CF.
static bool foldDiscriminantTest(MachineInstr &TEST, MachineFunction &MF,
                                 const TargetRegisterInfo *TRI) {
  MachineBasicBlock &MBB = *TEST.getParent();

  if (!TEST.getOperand(0).isReg() || !TEST.getOperand(1).isImm() ||
      TEST.getOperand(1).getImm() != 1)
    return false;

  Register DiscReg = TEST.getOperand(0).getReg();
  if (!DiscReg)
    return false;

  // Walk back to the setb that materialised the discriminant. Anything that
  // redefines the flags on the way disqualifies the fold: the branch would be
  // rewritten to read whatever flags are live at the branch, and those have to
  // still be the callee's carry-out.
  MachineInstr *SetB = nullptr;
  for (auto It = TEST.getIterator(); It != MBB.begin();) {
    --It;
    MachineInstr &MI = *It;
    if (MI.getOpcode() == X86::SETCCr && MI.getOperand(0).isReg() &&
        MI.getOperand(0).getReg() &&
        TRI->regsOverlap(MI.getOperand(0).getReg(), DiscReg) &&
        MI.getOperand(1).isImm() && MI.getOperand(1).getImm() == X86::COND_B) {
      SetB = &MI;
      break;
    }
    if (MI.modifiesRegister(X86::EFLAGS, TRI))
      return false;
    // A different definition of the same register means the setb is not what
    // the test sees.
    if (MI.getNumOperands() && MI.getOperand(0).isReg() &&
        MI.getOperand(0).getReg() &&
        TRI->regsOverlap(MI.getOperand(0).getReg(), DiscReg))
      return false;
  }
  if (!SetB)
    return false;

  // The first flag consumer after the test has to be the branch (or cmov) that
  // the test was feeding. If it is anything else the test's flags are read by
  // something we cannot rewrite, or the flags are redefined first.
  MachineInstr *Consumer = nullptr;
  X86::CondCode NewCC = X86::COND_INVALID;
  for (auto It = std::next(TEST.getIterator()); It != MBB.end(); ++It) {
    MachineInstr &MI = *It;
    if (MI.readsRegister(X86::EFLAGS, TRI)) {
      X86::CondCode CC = X86::getCondFromMI(MI);
      if ((CC == X86::COND_E || CC == X86::COND_NE) &&
          (MI.getOpcode() == X86::JCC_1 || X86::isCMOVCC(MI.getOpcode()))) {
        Consumer = &MI;
        // `testb $1, %disc` sets ZF iff the discriminant bit is clear, i.e. iff
        // CF was clear, so je becomes jae and jne becomes jb.
        NewCC = (CC == X86::COND_E) ? X86::COND_AE : X86::COND_B;
      }
      break;
    }
    if (MI.modifiesRegister(X86::EFLAGS, TRI))
      break;
    // Any other read of the discriminant means the register has to survive.
    if (MI.readsRegister(DiscReg, TRI))
      return false;
  }
  if (!Consumer)
    return false;

  const MCInstrDesc &Desc = Consumer->getDesc();
  int CondOpIdx = X86::getCondSrcNoFromDesc(Desc);
  if (CondOpIdx < 0)
    return false;

  // Rewrite the consumer before erasing anything, so a bail-out above can never
  // leave the block half-modified.
  Consumer->getOperand(CondOpIdx + Desc.getNumDefs()).setImm(NewCC);
  TEST.eraseFromParent();

  // The setb is dead once the test is gone, unless something else reads the
  // discriminant first. Note that the read has to be *after* the definition: a
  // register reused later for an unrelated value, or read earlier in the block
  // for the call's own setup, says nothing about this def.
  if (!isDefLive(*SetB, DiscReg, TRI))
    SetB->eraseFromParent();

  LLVM_DEBUG(dbgs() << "Herbception: folded discriminant test in "
                    << MF.getName() << '\n');
  ++NumFolded;
  return true;
}

bool X86HerbceptionFold::runOnMachineFunction(MachineFunction &MF) {
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    // Erasing the test invalidates the walk, so restart the block after each
    // fold. The pattern is rare, so the rescan costs nothing.
    bool Again = true;
    while (Again) {
      Again = false;
      for (auto II = MBB.begin(), IE = MBB.end(); II != IE; ++II) {
        MachineInstr &MI = *II;
        if (MI.getOpcode() != X86::TEST8ri)
          continue;
        if (foldDiscriminantTest(MI, MF, TRI)) {
          Changed = true;
          Again = true;
          break;
        }
      }
    }
  }

  return Changed;
}
