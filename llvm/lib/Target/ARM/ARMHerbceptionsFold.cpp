//===-- ARMHerbceptionsFold.cpp - Fold the throws discriminant test --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A herbceptions (throws) function returns its failure discriminant in the CPSR
// carry flag, and clang materialises it immediately after the call:
//
//   %z = MOVi 0
//   %d = ADCri %z, 0, implicit $cpsr      ; z + 0 + C, i.e. the discriminant
//   CMPri %d, 0, implicit-def $cpsr
//   Bcc ..., $cpsr
//
// When nothing redefines the flags in between, the materialisation and its
// compare collapse into the branch reading the carry the call left behind,
// removing three instructions from every throws call site:
//
//   BL @foo, ..., implicit-def $cpsr
//   Bcc ..., $cpsr                       ; cc rewritten onto C
//
// The same applies to a predicated select rather than a branch, which keeps the
// discriminant in a register and pays for the materialisation on every call
// site:
//
//   MOVi 0 / ADCri / CMPri
//   MOVCCi16 ..., cc, $cpsr              ->  MOVCCi16 ..., cc', $cpsr
//
// AArch64 gets the equivalent fold in AArch64MIPeepholeOpt (visitHERB_CSET and
// visitHERB_CSETTest).
//
// Placement
// ---------
// This runs before register allocation, which is the point at which the extra
// register the materialisation occupies can still be released to the allocator.
//
// Doing the fold before the frame is final is only sound if ARM's call-frame
// adjustment cannot clobber the flags the fold comes to depend on. It cannot:
// the adjustment is emitted by emitSPUpdate via emitARMRegPlusImmediate and
// emitT2RegPlusImmediate, which only ever build the non-flag-setting ADD/SUB
// forms (SUBri/ADDri, t2SUBri/t2ADDri, t2SUBspImm/t2ADDspImm, tSUBspi/tADDspi,
// t2SUBrr/t2ADDrr), and the ADJCALLSTACKUP pseudo it is expanded from declares
// SP alone. This is the property X86 did not have - its addl/subl redefined
// EFLAGS, which is why X86HerbceptionsFold has to wait for prolog/epilog
// insertion. The explicit CPSR-clobber check below keeps the fold honest should
// ARM's frame lowering ever start emitting an S-form, but note that it cannot
// see through a pseudo, so it is the invariant above that is load-bearing.
//
//===----------------------------------------------------------------------===//

#include "ARM.h"
#include "ARMBaseInstrInfo.h"
#include "ARMSubtarget.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "arm-herbceptions-fold"

STATISTIC(NumFolded, "Number of throws discriminant tests folded onto CPSR");

namespace {

class ARMHerbceptionsFold : public MachineFunctionPass {
public:
  static char ID;

  ARMHerbceptionsFold() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};

} // end anonymous namespace

char ARMHerbceptionsFold::ID = 0;

INITIALIZE_PASS(ARMHerbceptionsFold, DEBUG_TYPE,
                "Fold throws discriminant tests onto the carry flag", false,
                false)

FunctionPass *llvm::createARMHerbceptionsFoldPass() {
  return new ARMHerbceptionsFold();
}

/// The operand index of the condition code \p MI is predicated on, i.e. the
/// immediate operand just before the CPSR predicate register, or -1 when \p MI
/// is not predicated on CPSR.
static int getPredCCIndex(const MachineInstr &MI) {
  for (int I = MI.getNumOperands() - 1; I > 0; --I) {
    const MachineOperand &MO = MI.getOperand(I);
    if (!MO.isReg() || MO.getReg() != ARM::CPSR || !MO.isUse() ||
        MO.isImplicit())
      continue;
    const MachineOperand &CC = MI.getOperand(I - 1);
    // The compare itself writes CPSR with no condition of its own, so the
    // operand in front of the CPSR use has to look like a condition code.
    return CC.isImm() ? I - 1 : -1;
  }
  return -1;
}

/// The first register \p MI reads, or an invalid register if it reads none.
static Register getFirstSrcReg(const MachineInstr &MI) {
  for (const MachineOperand &MO : MI.operands()) {
    if (MO.isReg() && MO.isUse() && MO.getReg().isValid())
      return MO.getReg();
  }
  return Register();
}

/// The first immediate operand of \p MI. Returns false if there is none.
static bool getFirstImm(const MachineInstr &MI, int64_t &Imm) {
  for (const MachineOperand &MO : MI.operands()) {
    if (MO.isImm()) {
      Imm = MO.getImm();
      return true;
    }
  }
  return false;
}

static bool isCarryMaterialisation(unsigned Opc) {
  return Opc == ARM::ADCri || Opc == ARM::t2ADCri;
}

static bool isZeroMove(unsigned Opc) {
  return Opc == ARM::MOVi || Opc == ARM::t2MOVi;
}

static bool isDiscriminantCompare(unsigned Opc) {
  return Opc == ARM::CMPri || Opc == ARM::t2CMPri;
}

/// Fold the carry materialisation feeding \p Consumer into a test of live CPSR.
static bool foldDiscriminantTest(MachineInstr &Consumer, MachineRegisterInfo *MRI,
                                 const TargetRegisterInfo *TRI) {
  MachineBasicBlock &MBB = *Consumer.getParent();

  int CCOp = getPredCCIndex(Consumer);
  if (CCOp < 0)
    return false;
  int CC = Consumer.getOperand(CCOp).getImm();
  if (CC != ARMCC::EQ && CC != ARMCC::NE)
    return false;

  // Walk back to the instruction whose CPSR definition the consumer reads.
  // Anything else that reads CPSR on the way already depends on these flags,
  // and anything that writes them first means the consumer is not reading the
  // materialisation's zero test at all.
  MachineInstr *CPSRDef = nullptr;
  for (auto It = Consumer.getIterator(); It != MBB.begin();) {
    --It;
    if (It->modifiesRegister(ARM::CPSR, TRI)) {
      CPSRDef = &*It;
      break;
    }
    if (It->readsRegister(ARM::CPSR, TRI))
      return false;
  }
  if (!CPSRDef)
    return false;

  MachineInstr *ADC = nullptr;
  MachineInstr *Cmp = nullptr;
  // The discriminant is 0 or 1, and clang either tests it against zero or
  // against one depending on which arm of the branch the error path is on.
  // Both leave the answer in Z; they only differ in which way round.
  bool Inverted = false;

  if (isDiscriminantCompare(CPSRDef->getOpcode())) {
    // %d = ADCri %z, 0 / CMPri %d, {0,1}
    int64_t Imm;
    if (!getFirstImm(*CPSRDef, Imm) || (Imm != 0 && Imm != 1))
      return false;
    Inverted = Imm == 1;
    Register Src = getFirstSrcReg(*CPSRDef);
    ADC = Src ? MRI->getUniqueVRegDef(Src) : nullptr;
    if (!ADC || ADC->getParent() != &MBB)
      return false;
    Cmp = CPSRDef;
  } else {
    // The compare was already folded into the ADC, so the ADC itself defines
    // the CPSR the consumer reads.
    ADC = CPSRDef;
  }

  if (!isCarryMaterialisation(ADC->getOpcode()))
    return false;

  // The ADC has to be `0 + 0 + carry`, which is a plain copy of the carry bit;
  // any other addend would make the value something other than the
  // discriminant.
  int64_t ADCImm;
  if (!getFirstImm(*ADC, ADCImm) || ADCImm != 0)
    return false;
  Register ZeroReg = getFirstSrcReg(*ADC);
  MachineInstr *Zero = ZeroReg ? MRI->getUniqueVRegDef(ZeroReg) : nullptr;
  if (!Zero || Zero->getParent() != &MBB || !isZeroMove(Zero->getOpcode()))
    return false;
  int64_t ZeroImm;
  if (!getFirstImm(*Zero, ZeroImm) || ZeroImm != 0)
    return false;

  // Neither the zero nor the materialised discriminant may feed anything else.
  if (!MRI->hasOneNonDBGUse(ZeroReg))
    return false;
  Register ADCDst = ADC->getOperand(0).getReg();
  if (Cmp ? !MRI->hasOneNonDBGUse(ADCDst) : !MRI->use_nodbg_empty(ADCDst))
    return false;

  // The materialisation is the only definition of the flags the consumer
  // reads, and the fold removes it, so the consumer has to be their only
  // reader: any other instruction that tests them would quietly start testing
  // whatever CPSR the call left behind. Walking on to the next CPSR writer
  // bounds the search.
  unsigned Readers = 0;
  for (auto It = std::next(ADC->getIterator()); It != MBB.end(); ++It) {
    if (&*It == Cmp)
      continue;
    if (It->modifiesRegister(ARM::CPSR, TRI))
      break;
    if (It->readsRegister(ARM::CPSR, TRI)) {
      if (&*It != &Consumer)
        return false;
      ++Readers;
    }
  }
  if (Readers != 1)
    return false;

  // The materialised discriminant is the callee's carry-out, so the consumer's
  // zero test is really a test of that carry: eq means "carry clear" (lo) and
  // ne means "carry set" (hs) when the compare was against zero, and the other
  // way round when it was against one.
  bool IsEQ = CC == ARMCC::EQ;
  Consumer.getOperand(CCOp).setImm((IsEQ != Inverted) ? ARMCC::LO : ARMCC::HS);

  if (Cmp)
    Cmp->eraseFromParent();
  ADC->eraseFromParent();
  Zero->eraseFromParent();

  LLVM_DEBUG(dbgs() << "Herbceptions: folded discriminant test\n");
  ++NumFolded;
  return true;
}

bool ARMHerbceptionsFold::runOnMachineFunction(MachineFunction &MF) {
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  MachineRegisterInfo *MRI = &MF.getRegInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    // Erasing the materialisation invalidates the walk, so restart the block
    // after each fold. The pattern is rare, so the rescan costs nothing.
    bool Again = true;
    while (Again) {
      Again = false;
      for (MachineInstr &MI : make_early_inc_range(MBB)) {
        // Only instructions conditionally predicated on CPSR can be the
        // consumer; an `al` instruction reads no flags at all.
        if (getPredCCIndex(MI) < 0)
          continue;
        if (foldDiscriminantTest(MI, MRI, TRI)) {
          Changed = true;
          Again = true;
          break;
        }
      }
    }
  }

  return Changed;
}
