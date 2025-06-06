//===- X86RecognizableInstr.cpp - Disassembler instruction spec -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file is part of the X86 Disassembler Emitter.
// It contains the implementation of a single recognizable instruction.
// Documentation for the disassembler emitter in general can be found in
//  X86DisassemblerEmitter.h.
//
//===----------------------------------------------------------------------===//

#include "X86RecognizableInstr.h"
#include "X86DisassemblerShared.h"
#include "X86DisassemblerTables.h"
#include "X86ModRMFilters.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/TableGen/Record.h"
#include <string>

using namespace llvm;
using namespace X86Disassembler;

std::string X86Disassembler::getMnemonic(const CodeGenInstruction *I,
                                         unsigned Variant) {
  // Extract a mnemonic assuming it's separated by \t
  std::string Mnemonic =
      StringRef(I->FlattenAsmStringVariants(I->AsmString, Variant))
          .take_until([](char C) { return C == '\t'; })
          .str();

  // Special case: CMOVCC, JCC, SETCC, CMPCCXADD have "${cond}" in mnemonic.
  // Replace it with "CC" in-place.
  auto CondPos = Mnemonic.find("${cond}");
  if (CondPos != std::string::npos)
    Mnemonic = Mnemonic.replace(CondPos, 7, "CC");
  return StringRef(Mnemonic).upper();
}

bool X86Disassembler::isRegisterOperand(const Record *Rec) {
  return Rec->isSubClassOf("RegisterClass") ||
         Rec->isSubClassOf("RegisterOperand");
}

bool X86Disassembler::isMemoryOperand(const Record *Rec) {
  return Rec->isSubClassOf("Operand") &&
         Rec->getValueAsString("OperandType") == "OPERAND_MEMORY";
}

bool X86Disassembler::isImmediateOperand(const Record *Rec) {
  return Rec->isSubClassOf("Operand") &&
         Rec->getValueAsString("OperandType") == "OPERAND_IMMEDIATE";
}

unsigned X86Disassembler::getRegOperandSize(const Record *RegRec) {
  if (RegRec->isSubClassOf("RegisterClass"))
    return RegRec->getValueAsInt("Alignment");
  if (RegRec->isSubClassOf("RegisterOperand"))
    return RegRec->getValueAsDef("RegClass")->getValueAsInt("Alignment");

  llvm_unreachable("Register operand's size not known!");
}

unsigned X86Disassembler::getMemOperandSize(const Record *MemRec) {
  if (MemRec->isSubClassOf("X86MemOperand"))
    return MemRec->getValueAsInt("Size");

  llvm_unreachable("Memory operand's size not known!");
}

/// byteFromBitsInit - Extracts a value at most 8 bits in width from a BitsInit.
///   Useful for switch statements and the like.
///
/// @param init - A reference to the BitsInit to be decoded.
/// @return     - The field, with the first bit in the BitsInit as the lowest
///               order bit.
static uint8_t byteFromBitsInit(const BitsInit &init) {
  int width = init.getNumBits();

  assert(width <= 8 && "Field is too large for uint8_t!");

  uint8_t mask = 0x01;
  uint8_t ret = 0;

  for (int index = 0; index < width; index++) {
    if (cast<BitInit>(init.getBit(index))->getValue())
      ret |= mask;

    mask <<= 1;
  }

  return ret;
}

/// byteFromRec - Extract a value at most 8 bits in with from a Record given the
///   name of the field.
///
/// @param rec  - The record from which to extract the value.
/// @param name - The name of the field in the record.
/// @return     - The field, as translated by byteFromBitsInit().
static uint8_t byteFromRec(const Record *rec, StringRef name) {
  const BitsInit *bits = rec->getValueAsBitsInit(name);
  return byteFromBitsInit(*bits);
}

RecognizableInstrBase::RecognizableInstrBase(const CodeGenInstruction &insn) {
  const Record *Rec = insn.TheDef;
  assert(Rec->isSubClassOf("X86Inst") && "Not a X86 Instruction");
  OpPrefix = byteFromRec(Rec, "OpPrefixBits");
  OpMap = byteFromRec(Rec, "OpMapBits");
  Opcode = byteFromRec(Rec, "Opcode");
  Form = byteFromRec(Rec, "FormBits");
  Encoding = byteFromRec(Rec, "OpEncBits");
  OpSize = byteFromRec(Rec, "OpSizeBits");
  AdSize = byteFromRec(Rec, "AdSizeBits");
  HasREX_W = Rec->getValueAsBit("hasREX_W");
  HasVEX_4V = Rec->getValueAsBit("hasVEX_4V");
  IgnoresW = Rec->getValueAsBit("IgnoresW");
  IgnoresVEX_L = Rec->getValueAsBit("ignoresVEX_L");
  HasEVEX_L2 = Rec->getValueAsBit("hasEVEX_L2");
  HasEVEX_K = Rec->getValueAsBit("hasEVEX_K");
  HasEVEX_KZ = Rec->getValueAsBit("hasEVEX_Z");
  HasEVEX_B = Rec->getValueAsBit("hasEVEX_B");
  HasEVEX_U = Rec->getValueAsBit("hasEVEX_U");
  HasEVEX_NF = Rec->getValueAsBit("hasEVEX_NF");
  HasTwoConditionalOps = Rec->getValueAsBit("hasTwoConditionalOps");
  IsCodeGenOnly = Rec->getValueAsBit("isCodeGenOnly");
  IsAsmParserOnly = Rec->getValueAsBit("isAsmParserOnly");
  ForceDisassemble = Rec->getValueAsBit("ForceDisassemble");
  CD8_Scale = byteFromRec(Rec, "CD8_Scale");
  HasVEX_L = Rec->getValueAsBit("hasVEX_L");
  ExplicitREX2Prefix =
      byteFromRec(Rec, "explicitOpPrefixBits") == X86Local::ExplicitREX2;

  EncodeRC = HasEVEX_B &&
             (Form == X86Local::MRMDestReg || Form == X86Local::MRMSrcReg);
}

bool RecognizableInstrBase::shouldBeEmitted() const {
  return Form != X86Local::Pseudo && (!IsCodeGenOnly || ForceDisassemble) &&
         !IsAsmParserOnly;
}

RecognizableInstr::RecognizableInstr(DisassemblerTables &tables,
                                     const CodeGenInstruction &insn,
                                     InstrUID uid)
    : RecognizableInstrBase(insn), Rec(insn.TheDef), Name(Rec->getName().str()),
      Is32Bit(false), Is64Bit(false), Operands(&insn.Operands.OperandList),
      UID(uid), Spec(&tables.specForUID(uid)) {
  // Check for 64-bit inst which does not require REX
  // FIXME: Is there some better way to check for In64BitMode?
  for (const Record *Predicate : Rec->getValueAsListOfDefs("Predicates")) {
    if (Predicate->getName().contains("Not64Bit") ||
        Predicate->getName().contains("In32Bit")) {
      Is32Bit = true;
      break;
    }
    if (Predicate->getName().contains("In64Bit")) {
      Is64Bit = true;
      break;
    }
  }
}

void RecognizableInstr::processInstr(DisassemblerTables &tables,
                                     const CodeGenInstruction &insn,
                                     InstrUID uid) {
  if (!insn.TheDef->isSubClassOf("X86Inst"))
    return;
  RecognizableInstr recogInstr(tables, insn, uid);

  if (!recogInstr.shouldBeEmitted())
    return;
  recogInstr.emitInstructionSpecifier();
  recogInstr.emitDecodePath(tables);
}

#define EVEX_KB(n)                                                             \
  (HasEVEX_KZ && HasEVEX_B                                                     \
       ? n##_KZ_B                                                              \
       : (HasEVEX_K && HasEVEX_B                                               \
              ? n##_K_B                                                        \
              : (HasEVEX_KZ ? n##_KZ                                           \
                            : (HasEVEX_K ? n##_K : (HasEVEX_B ? n##_B : n)))))

#define EVEX_NF(n) (HasEVEX_NF ? n##_NF : n)
#define EVEX_B_NF(n) (HasEVEX_B ? EVEX_NF(n##_B) : EVEX_NF(n))
#define EVEX_KB_ADSIZE(n) AdSize == X86Local::AdSize32 ? n##_ADSIZE : EVEX_KB(n)
#define EVEX_KB_U(n)                                                           \
  (HasEVEX_KZ ? n##_KZ_B_U : (HasEVEX_K ? n##_K_B_U : n##_B_U))

InstructionContext RecognizableInstr::insnContext() const {
  InstructionContext insnContext;

  if (Encoding == X86Local::EVEX) {
    if (HasVEX_L && HasEVEX_L2) {
      errs() << "Don't support VEX.L if EVEX_L2 is enabled: " << Name << "\n";
      llvm_unreachable("Don't support VEX.L if EVEX_L2 is enabled");
    }
    if (EncodeRC && HasEVEX_U) {
      // EVEX_U
      if (HasREX_W) {
        if (OpPrefix == X86Local::PD)
          insnContext = EVEX_KB_U(IC_EVEX_W_OPSIZE);
        else if (OpPrefix == X86Local::XS)
          insnContext = EVEX_KB_U(IC_EVEX_W_XS);
        else if (OpPrefix == X86Local::XD)
          insnContext = EVEX_KB_U(IC_EVEX_W_XD);
        else if (OpPrefix == X86Local::PS)
          insnContext = EVEX_KB_U(IC_EVEX_W);
        else {
          errs() << "Instruction does not use a prefix: " << Name << "\n";
          llvm_unreachable("Invalid prefix");
        }
      } else {
        if (OpPrefix == X86Local::PD)
          insnContext = EVEX_KB_U(IC_EVEX_OPSIZE);
        else if (OpPrefix == X86Local::XS)
          insnContext = EVEX_KB_U(IC_EVEX_XS);
        else if (OpPrefix == X86Local::XD)
          insnContext = EVEX_KB_U(IC_EVEX_XD);
        else if (OpPrefix == X86Local::PS)
          insnContext = EVEX_KB_U(IC_EVEX);
        else {
          errs() << "Instruction does not use a prefix: " << Name << "\n";
          llvm_unreachable("Invalid prefix");
        }
      }
    } else if (HasEVEX_NF) {
      if (OpPrefix == X86Local::PD)
        insnContext = EVEX_B_NF(IC_EVEX_OPSIZE);
      else if (HasREX_W)
        insnContext = EVEX_B_NF(IC_EVEX_W);
      else
        insnContext = EVEX_B_NF(IC_EVEX);
    } else if (!EncodeRC && HasVEX_L && HasREX_W) {
      // VEX_L & VEX_W
      if (OpPrefix == X86Local::PD)
        insnContext = EVEX_KB(IC_EVEX_L_W_OPSIZE);
      else if (OpPrefix == X86Local::XS)
        insnContext = EVEX_KB(IC_EVEX_L_W_XS);
      else if (OpPrefix == X86Local::XD)
        insnContext = EVEX_KB(IC_EVEX_L_W_XD);
      else if (OpPrefix == X86Local::PS)
        insnContext = EVEX_KB(IC_EVEX_L_W);
      else {
        errs() << "Instruction does not use a prefix: " << Name << "\n";
        llvm_unreachable("Invalid prefix");
      }
    } else if (!EncodeRC && HasVEX_L) {
      // VEX_L
      if (OpPrefix == X86Local::PD)
        insnContext = EVEX_KB(IC_EVEX_L_OPSIZE);
      else if (OpPrefix == X86Local::XS)
        insnContext = EVEX_KB(IC_EVEX_L_XS);
      else if (OpPrefix == X86Local::XD)
        insnContext = EVEX_KB(IC_EVEX_L_XD);
      else if (OpPrefix == X86Local::PS)
        insnContext = EVEX_KB(IC_EVEX_L);
      else {
        errs() << "Instruction does not use a prefix: " << Name << "\n";
        llvm_unreachable("Invalid prefix");
      }
    } else if (!EncodeRC && HasEVEX_L2 && HasREX_W) {
      // EVEX_L2 & VEX_W
      if (OpPrefix == X86Local::PD)
        insnContext = EVEX_KB(IC_EVEX_L2_W_OPSIZE);
      else if (OpPrefix == X86Local::XS)
        insnContext = EVEX_KB(IC_EVEX_L2_W_XS);
      else if (OpPrefix == X86Local::XD)
        insnContext = EVEX_KB(IC_EVEX_L2_W_XD);
      else if (OpPrefix == X86Local::PS)
        insnContext = EVEX_KB(IC_EVEX_L2_W);
      else {
        errs() << "Instruction does not use a prefix: " << Name << "\n";
        llvm_unreachable("Invalid prefix");
      }
    } else if (!EncodeRC && HasEVEX_L2) {
      // EVEX_L2
      if (OpPrefix == X86Local::PD)
        insnContext = EVEX_KB(IC_EVEX_L2_OPSIZE);
      else if (OpPrefix == X86Local::XD)
        insnContext = EVEX_KB(IC_EVEX_L2_XD);
      else if (OpPrefix == X86Local::XS)
        insnContext = EVEX_KB(IC_EVEX_L2_XS);
      else if (OpPrefix == X86Local::PS)
        insnContext = EVEX_KB(IC_EVEX_L2);
      else {
        errs() << "Instruction does not use a prefix: " << Name << "\n";
        llvm_unreachable("Invalid prefix");
      }
    } else if (HasREX_W) {
      // VEX_W
      if (OpPrefix == X86Local::PD)
        insnContext = EVEX_KB(IC_EVEX_W_OPSIZE);
      else if (OpPrefix == X86Local::XS)
        insnContext = EVEX_KB(IC_EVEX_W_XS);
      else if (OpPrefix == X86Local::XD)
        insnContext = EVEX_KB(IC_EVEX_W_XD);
      else if (OpPrefix == X86Local::PS)
        insnContext = EVEX_KB(IC_EVEX_W);
      else {
        errs() << "Instruction does not use a prefix: " << Name << "\n";
        llvm_unreachable("Invalid prefix");
      }
    }
    // No L, no W
    else if (OpPrefix == X86Local::PD) {
      insnContext = EVEX_KB_ADSIZE(IC_EVEX_OPSIZE);
    } else if (OpPrefix == X86Local::XD)
      insnContext = EVEX_KB_ADSIZE(IC_EVEX_XD);
    else if (OpPrefix == X86Local::XS)
      insnContext = EVEX_KB_ADSIZE(IC_EVEX_XS);
    else if (OpPrefix == X86Local::PS)
      insnContext = EVEX_KB(IC_EVEX);
    else {
      errs() << "Instruction does not use a prefix: " << Name << "\n";
      llvm_unreachable("Invalid prefix");
    }
    /// eof EVEX
  } else if (Encoding == X86Local::VEX || Encoding == X86Local::XOP) {
    if (HasVEX_L && HasREX_W) {
      if (OpPrefix == X86Local::PD)
        insnContext = IC_VEX_L_W_OPSIZE;
      else if (OpPrefix == X86Local::XS)
        insnContext = IC_VEX_L_W_XS;
      else if (OpPrefix == X86Local::XD)
        insnContext = IC_VEX_L_W_XD;
      else if (OpPrefix == X86Local::PS)
        insnContext = IC_VEX_L_W;
      else {
        errs() << "Instruction does not use a prefix: " << Name << "\n";
        llvm_unreachable("Invalid prefix");
      }
    } else if (OpPrefix == X86Local::PD && HasVEX_L)
      insnContext = IC_VEX_L_OPSIZE;
    else if (OpPrefix == X86Local::PD && HasREX_W)
      insnContext = IC_VEX_W_OPSIZE;
    else if (OpPrefix == X86Local::PD)
      insnContext = IC_VEX_OPSIZE;
    else if (HasVEX_L && OpPrefix == X86Local::XS)
      insnContext = IC_VEX_L_XS;
    else if (HasVEX_L && OpPrefix == X86Local::XD)
      insnContext = IC_VEX_L_XD;
    else if (HasREX_W && OpPrefix == X86Local::XS)
      insnContext = IC_VEX_W_XS;
    else if (HasREX_W && OpPrefix == X86Local::XD)
      insnContext = IC_VEX_W_XD;
    else if (HasREX_W && OpPrefix == X86Local::PS)
      insnContext = IC_VEX_W;
    else if (HasVEX_L && OpPrefix == X86Local::PS)
      insnContext = IC_VEX_L;
    else if (OpPrefix == X86Local::XD)
      insnContext = IC_VEX_XD;
    else if (OpPrefix == X86Local::XS)
      insnContext = IC_VEX_XS;
    else if (OpPrefix == X86Local::PS)
      insnContext = IC_VEX;
    else {
      errs() << "Instruction does not use a prefix: " << Name << "\n";
      llvm_unreachable("Invalid prefix");
    }
  } else if (Is64Bit || HasREX_W || AdSize == X86Local::AdSize64) {
    if (HasREX_W && (OpSize == X86Local::OpSize16 || OpPrefix == X86Local::PD))
      insnContext = IC_64BIT_REXW_OPSIZE;
    else if (HasREX_W && AdSize == X86Local::AdSize32)
      insnContext = IC_64BIT_REXW_ADSIZE;
    else if (OpSize == X86Local::OpSize16 && OpPrefix == X86Local::XD)
      insnContext = IC_64BIT_XD_OPSIZE;
    else if (OpSize == X86Local::OpSize16 && OpPrefix == X86Local::XS)
      insnContext = IC_64BIT_XS_OPSIZE;
    else if (AdSize == X86Local::AdSize32 && OpPrefix == X86Local::PD)
      insnContext = IC_64BIT_OPSIZE_ADSIZE;
    else if (OpSize == X86Local::OpSize16 && AdSize == X86Local::AdSize32)
      insnContext = IC_64BIT_OPSIZE_ADSIZE;
    else if (OpSize == X86Local::OpSize16 || OpPrefix == X86Local::PD)
      insnContext = IC_64BIT_OPSIZE;
    else if (AdSize == X86Local::AdSize32)
      insnContext = IC_64BIT_ADSIZE;
    else if (HasREX_W && OpPrefix == X86Local::XS)
      insnContext = IC_64BIT_REXW_XS;
    else if (HasREX_W && OpPrefix == X86Local::XD)
      insnContext = IC_64BIT_REXW_XD;
    else if (OpPrefix == X86Local::XD)
      insnContext = IC_64BIT_XD;
    else if (OpPrefix == X86Local::XS)
      insnContext = IC_64BIT_XS;
    else if (ExplicitREX2Prefix)
      insnContext = IC_64BIT_REX2;
    else if (HasREX_W)
      insnContext = IC_64BIT_REXW;
    else
      insnContext = IC_64BIT;
  } else {
    if (OpSize == X86Local::OpSize16 && OpPrefix == X86Local::XD)
      insnContext = IC_XD_OPSIZE;
    else if (OpSize == X86Local::OpSize16 && OpPrefix == X86Local::XS)
      insnContext = IC_XS_OPSIZE;
    else if (AdSize == X86Local::AdSize16 && OpPrefix == X86Local::XD)
      insnContext = IC_XD_ADSIZE;
    else if (AdSize == X86Local::AdSize16 && OpPrefix == X86Local::XS)
      insnContext = IC_XS_ADSIZE;
    else if (AdSize == X86Local::AdSize16 && OpPrefix == X86Local::PD)
      insnContext = IC_OPSIZE_ADSIZE;
    else if (OpSize == X86Local::OpSize16 && AdSize == X86Local::AdSize16)
      insnContext = IC_OPSIZE_ADSIZE;
    else if (OpSize == X86Local::OpSize16 || OpPrefix == X86Local::PD)
      insnContext = IC_OPSIZE;
    else if (AdSize == X86Local::AdSize16)
      insnContext = IC_ADSIZE;
    else if (OpPrefix == X86Local::XD)
      insnContext = IC_XD;
    else if (OpPrefix == X86Local::XS)
      insnContext = IC_XS;
    else
      insnContext = IC;
  }

  return insnContext;
}

void RecognizableInstr::adjustOperandEncoding(OperandEncoding &encoding) {
  // The scaling factor for AVX512 compressed displacement encoding is an
  // instruction attribute.  Adjust the ModRM encoding type to include the
  // scale for compressed displacement.
  if ((encoding != ENCODING_RM && encoding != ENCODING_VSIB &&
       encoding != ENCODING_SIB) ||
      CD8_Scale == 0)
    return;
  encoding = (OperandEncoding)(encoding + Log2_32(CD8_Scale));
  assert(((encoding >= ENCODING_RM && encoding <= ENCODING_RM_CD64) ||
          (encoding == ENCODING_SIB) ||
          (encoding >= ENCODING_VSIB && encoding <= ENCODING_VSIB_CD64)) &&
         "Invalid CDisp scaling");
}

void RecognizableInstr::handleOperand(bool optional, unsigned &operandIndex,
                                      unsigned &physicalOperandIndex,
                                      unsigned numPhysicalOperands,
                                      const unsigned *operandMapping,
                                      EncodingFn encodingFromString) {
  if (optional) {
    if (physicalOperandIndex >= numPhysicalOperands)
      return;
  } else {
    assert(physicalOperandIndex < numPhysicalOperands);
  }

  while (operandMapping[operandIndex] != operandIndex) {
    Spec->operands[operandIndex].encoding = ENCODING_DUP;
    Spec->operands[operandIndex].type =
        (OperandType)(TYPE_DUP0 + operandMapping[operandIndex]);
    ++operandIndex;
  }

  StringRef typeName = (*Operands)[operandIndex].Rec->getName();

  OperandEncoding encoding = encodingFromString(typeName, OpSize);
  // Adjust the encoding type for an operand based on the instruction.
  adjustOperandEncoding(encoding);
  Spec->operands[operandIndex].encoding = encoding;
  Spec->operands[operandIndex].type =
      typeFromString(typeName, HasREX_W, OpSize);

  ++operandIndex;
  ++physicalOperandIndex;
}

void RecognizableInstr::emitInstructionSpecifier() {
  Spec->name = Name;

  Spec->insnContext = insnContext();

  const std::vector<CGIOperandList::OperandInfo> &OperandList = *Operands;

  unsigned numOperands = OperandList.size();
  unsigned numPhysicalOperands = 0;

  // operandMapping maps from operands in OperandList to their originals.
  // If operandMapping[i] != i, then the entry is a duplicate.
  unsigned operandMapping[X86_MAX_OPERANDS];
  assert(numOperands <= X86_MAX_OPERANDS &&
         "X86_MAX_OPERANDS is not large enough");

  for (unsigned operandIndex = 0; operandIndex < numOperands; ++operandIndex) {
    if (!OperandList[operandIndex].Constraints.empty()) {
      const CGIOperandList::ConstraintInfo &Constraint =
          OperandList[operandIndex].Constraints[0];
      if (Constraint.isTied()) {
        operandMapping[operandIndex] = operandIndex;
        operandMapping[Constraint.getTiedOperand()] = operandIndex;
      } else {
        ++numPhysicalOperands;
        operandMapping[operandIndex] = operandIndex;
      }
    } else {
      ++numPhysicalOperands;
      operandMapping[operandIndex] = operandIndex;
    }
  }

#define HANDLE_OPERAND(class)                                                  \
  handleOperand(false, operandIndex, physicalOperandIndex,                     \
                numPhysicalOperands, operandMapping,                           \
                class##EncodingFromString);

#define HANDLE_OPTIONAL(class)                                                 \
  handleOperand(true, operandIndex, physicalOperandIndex, numPhysicalOperands, \
                operandMapping, class##EncodingFromString);

  // operandIndex should always be < numOperands
  unsigned operandIndex = 0;
  // physicalOperandIndex should always be < numPhysicalOperands
  unsigned physicalOperandIndex = 0;

#ifndef NDEBUG
  // Given the set of prefix bits, how many additional operands does the
  // instruction have?
  unsigned additionalOperands = 0;
  if (HasVEX_4V)
    ++additionalOperands;
  if (HasEVEX_K)
    ++additionalOperands;
  if (HasTwoConditionalOps)
    additionalOperands += 2;
#endif

  bool IsND = OpMap == X86Local::T_MAP4 && HasEVEX_B && HasVEX_4V;
  switch (Form) {
  default:
    llvm_unreachable("Unhandled form");
  case X86Local::PrefixByte:
    return;
  case X86Local::RawFrmSrc:
    HANDLE_OPERAND(relocation);
    return;
  case X86Local::RawFrmDst:
    HANDLE_OPERAND(relocation);
    return;
  case X86Local::RawFrmDstSrc:
    HANDLE_OPERAND(relocation);
    HANDLE_OPERAND(relocation);
    return;
  case X86Local::RawFrm:
    // Operand 1 (optional) is an address or immediate.
    assert(numPhysicalOperands <= 1 &&
           "Unexpected number of operands for RawFrm");
    HANDLE_OPTIONAL(relocation)
    break;
  case X86Local::RawFrmMemOffs:
    // Operand 1 is an address.
    HANDLE_OPERAND(relocation);
    break;
  case X86Local::AddRegFrm:
    // Operand 1 is added to the opcode.
    // Operand 2 (optional) is an address.
    assert(numPhysicalOperands >= 1 && numPhysicalOperands <= 2 &&
           "Unexpected number of operands for AddRegFrm");
    HANDLE_OPERAND(opcodeModifier)
    HANDLE_OPTIONAL(relocation)
    break;
  case X86Local::AddCCFrm:
    // Operand 1 (optional) is an address or immediate.
    assert(numPhysicalOperands == 2 &&
           "Unexpected number of operands for AddCCFrm");
    HANDLE_OPERAND(relocation)
    HANDLE_OPERAND(opcodeModifier)
    break;
  case X86Local::MRMDestRegCC:
    assert(numPhysicalOperands == 3 &&
           "Unexpected number of operands for MRMDestRegCC");
    HANDLE_OPERAND(rmRegister)
    HANDLE_OPERAND(roRegister)
    HANDLE_OPERAND(opcodeModifier)
    break;
  case X86Local::MRMDestReg:
    // Operand 1 is a register operand in the R/M field.
    // - In AVX512 there may be a mask operand here -
    // Operand 2 is a register operand in the Reg/Opcode field.
    // - In AVX, there is a register operand in the VEX.vvvv field here -
    // Operand 3 (optional) is an immediate.
    assert(numPhysicalOperands >= 2 + additionalOperands &&
           numPhysicalOperands <= 3 + additionalOperands &&
           "Unexpected number of operands for MRMDestReg");

    if (IsND)
      HANDLE_OPERAND(vvvvRegister)

    HANDLE_OPERAND(rmRegister)
    if (HasEVEX_K)
      HANDLE_OPERAND(writemaskRegister)

    if (!IsND && HasVEX_4V)
      // FIXME: In AVX, the register below becomes the one encoded
      // in ModRMVEX and the one above the one in the VEX.VVVV field
      HANDLE_OPERAND(vvvvRegister)

    HANDLE_OPERAND(roRegister)
    HANDLE_OPTIONAL(immediate)
    HANDLE_OPTIONAL(immediate)
    break;
  case X86Local::MRMDestMemCC:
    assert(numPhysicalOperands == 3 &&
           "Unexpected number of operands for MRMDestMemCC");
    HANDLE_OPERAND(memory)
    HANDLE_OPERAND(roRegister)
    HANDLE_OPERAND(opcodeModifier)
    break;
  case X86Local::MRMDestMem4VOp3CC:
    // Operand 1 is a register operand in the Reg/Opcode field.
    // Operand 2 is a register operand in the R/M field.
    // Operand 3 is VEX.vvvv
    // Operand 4 is condition code.
    assert(numPhysicalOperands == 4 &&
           "Unexpected number of operands for MRMDestMem4VOp3CC");
    HANDLE_OPERAND(roRegister)
    HANDLE_OPERAND(memory)
    HANDLE_OPERAND(vvvvRegister)
    HANDLE_OPERAND(opcodeModifier)
    break;
  case X86Local::MRMDestMem:
  case X86Local::MRMDestMemFSIB:
    // Operand 1 is a memory operand (possibly SIB-extended)
    // Operand 2 is a register operand in the Reg/Opcode field.
    // - In AVX, there is a register operand in the VEX.vvvv field here -
    // Operand 3 (optional) is an immediate.
    assert(numPhysicalOperands >= 2 + additionalOperands &&
           numPhysicalOperands <= 3 + additionalOperands &&
           "Unexpected number of operands for MRMDestMemFrm with VEX_4V");

    if (IsND)
      HANDLE_OPERAND(vvvvRegister)

    HANDLE_OPERAND(memory)

    if (HasEVEX_K)
      HANDLE_OPERAND(writemaskRegister)

    if (!IsND && HasVEX_4V)
      // FIXME: In AVX, the register below becomes the one encoded
      // in ModRMVEX and the one above the one in the VEX.VVVV field
      HANDLE_OPERAND(vvvvRegister)

    HANDLE_OPERAND(roRegister)
    HANDLE_OPTIONAL(immediate)
    HANDLE_OPTIONAL(immediate)
    break;
  case X86Local::MRMSrcReg:
    // Operand 1 is a register operand in the Reg/Opcode field.
    // Operand 2 is a register operand in the R/M field.
    // - In AVX, there is a register operand in the VEX.vvvv field here -
    // Operand 3 (optional) is an immediate.
    // Operand 4 (optional) is an immediate.

    assert(numPhysicalOperands >= 2 + additionalOperands &&
           numPhysicalOperands <= 4 + additionalOperands &&
           "Unexpected number of operands for MRMSrcRegFrm");

    if (IsND)
      HANDLE_OPERAND(vvvvRegister)

    HANDLE_OPERAND(roRegister)

    if (HasEVEX_K)
      HANDLE_OPERAND(writemaskRegister)

    if (!IsND && HasVEX_4V)
      // FIXME: In AVX, the register below becomes the one encoded
      // in ModRMVEX and the one above the one in the VEX.VVVV field
      HANDLE_OPERAND(vvvvRegister)

    HANDLE_OPERAND(rmRegister)
    HANDLE_OPTIONAL(immediate)
    HANDLE_OPTIONAL(immediate) // above might be a register in 7:4
    break;
  case X86Local::MRMSrcReg4VOp3:
    assert(numPhysicalOperands == 3 &&
           "Unexpected number of operands for MRMSrcReg4VOp3Frm");
    HANDLE_OPERAND(roRegister)
    HANDLE_OPERAND(rmRegister)
    HANDLE_OPERAND(vvvvRegister)
    break;
  case X86Local::MRMSrcRegOp4:
    assert(numPhysicalOperands >= 4 && numPhysicalOperands <= 5 &&
           "Unexpected number of operands for MRMSrcRegOp4Frm");
    HANDLE_OPERAND(roRegister)
    HANDLE_OPERAND(vvvvRegister)
    HANDLE_OPERAND(immediate) // Register in imm[7:4]
    HANDLE_OPERAND(rmRegister)
    HANDLE_OPTIONAL(immediate)
    break;
  case X86Local::MRMSrcRegCC:
    assert(numPhysicalOperands >= 3 && numPhysicalOperands <= 4 &&
           "Unexpected number of operands for MRMSrcRegCC");
    if (IsND)
      HANDLE_OPERAND(vvvvRegister)
    HANDLE_OPERAND(roRegister)
    HANDLE_OPERAND(rmRegister)
    HANDLE_OPERAND(opcodeModifier)
    break;
  case X86Local::MRMSrcMem:
  case X86Local::MRMSrcMemFSIB:
    // Operand 1 is a register operand in the Reg/Opcode field.
    // Operand 2 is a memory operand (possibly SIB-extended)
    // - In AVX, there is a register operand in the VEX.vvvv field here -
    // Operand 3 (optional) is an immediate.

    assert(numPhysicalOperands >= 2 + additionalOperands &&
           numPhysicalOperands <= 4 + additionalOperands &&
           "Unexpected number of operands for MRMSrcMemFrm");
    if (IsND)
      HANDLE_OPERAND(vvvvRegister)

    HANDLE_OPERAND(roRegister)

    if (HasEVEX_K)
      HANDLE_OPERAND(writemaskRegister)

    if (!IsND && HasVEX_4V)
      // FIXME: In AVX, the register below becomes the one encoded
      // in ModRMVEX and the one above the one in the VEX.VVVV field
      HANDLE_OPERAND(vvvvRegister)

    HANDLE_OPERAND(memory)
    HANDLE_OPTIONAL(immediate)
    HANDLE_OPTIONAL(immediate) // above might be a register in 7:4
    break;
  case X86Local::MRMSrcMem4VOp3:
    assert(numPhysicalOperands == 3 &&
           "Unexpected number of operands for MRMSrcMem4VOp3Frm");
    HANDLE_OPERAND(roRegister)
    HANDLE_OPERAND(memory)
    HANDLE_OPERAND(vvvvRegister)
    break;
  case X86Local::MRMSrcMemOp4:
    assert(numPhysicalOperands >= 4 && numPhysicalOperands <= 5 &&
           "Unexpected number of operands for MRMSrcMemOp4Frm");
    HANDLE_OPERAND(roRegister)
    HANDLE_OPERAND(vvvvRegister)
    HANDLE_OPERAND(immediate) // Register in imm[7:4]
    HANDLE_OPERAND(memory)
    HANDLE_OPTIONAL(immediate)
    break;
  case X86Local::MRMSrcMemCC:
    assert(numPhysicalOperands >= 3 && numPhysicalOperands <= 4 &&
           "Unexpected number of operands for MRMSrcMemCC");
    if (IsND)
      HANDLE_OPERAND(vvvvRegister)
    HANDLE_OPERAND(roRegister)
    HANDLE_OPERAND(memory)
    HANDLE_OPERAND(opcodeModifier)
    break;
  case X86Local::MRMXrCC:
    assert(numPhysicalOperands == 2 &&
           "Unexpected number of operands for MRMXrCC");
    HANDLE_OPERAND(rmRegister)
    HANDLE_OPERAND(opcodeModifier)
    break;
  case X86Local::MRMr0:
    // Operand 1 is a register operand in the R/M field.
    HANDLE_OPERAND(roRegister)
    break;
  case X86Local::MRMXr:
  case X86Local::MRM0r:
  case X86Local::MRM1r:
  case X86Local::MRM2r:
  case X86Local::MRM3r:
  case X86Local::MRM4r:
  case X86Local::MRM5r:
  case X86Local::MRM6r:
  case X86Local::MRM7r:
    // Operand 1 is a register operand in the R/M field.
    // Operand 2 (optional) is an immediate or relocation.
    // Operand 3 (optional) is an immediate.
    assert(numPhysicalOperands >= 0 + additionalOperands &&
           numPhysicalOperands <= 3 + additionalOperands &&
           "Unexpected number of operands for MRMnr");

    if (HasVEX_4V)
      HANDLE_OPERAND(vvvvRegister)

    if (HasEVEX_K)
      HANDLE_OPERAND(writemaskRegister)
    HANDLE_OPTIONAL(rmRegister)
    HANDLE_OPTIONAL(relocation)
    HANDLE_OPTIONAL(immediate)
    HANDLE_OPTIONAL(immediate)
    break;
  case X86Local::MRMXmCC:
    assert(numPhysicalOperands == 2 &&
           "Unexpected number of operands for MRMXm");
    HANDLE_OPERAND(memory)
    HANDLE_OPERAND(opcodeModifier)
    break;
  case X86Local::MRMXm:
  case X86Local::MRM0m:
  case X86Local::MRM1m:
  case X86Local::MRM2m:
  case X86Local::MRM3m:
  case X86Local::MRM4m:
  case X86Local::MRM5m:
  case X86Local::MRM6m:
  case X86Local::MRM7m:
    // Operand 1 is a memory operand (possibly SIB-extended)
    // Operand 2 (optional) is an immediate or relocation.
    assert(numPhysicalOperands >= 1 + additionalOperands &&
           numPhysicalOperands <= 2 + additionalOperands &&
           "Unexpected number of operands for MRMnm");

    if (HasVEX_4V)
      HANDLE_OPERAND(vvvvRegister)
    if (HasEVEX_K)
      HANDLE_OPERAND(writemaskRegister)
    HANDLE_OPERAND(memory)
    HANDLE_OPTIONAL(relocation)
    HANDLE_OPTIONAL(immediate)
    HANDLE_OPTIONAL(immediate)
    break;
  case X86Local::RawFrmImm8:
    // operand 1 is a 16-bit immediate
    // operand 2 is an 8-bit immediate
    assert(numPhysicalOperands == 2 &&
           "Unexpected number of operands for X86Local::RawFrmImm8");
    HANDLE_OPERAND(immediate)
    HANDLE_OPERAND(immediate)
    break;
  case X86Local::RawFrmImm16:
    // operand 1 is a 16-bit immediate
    // operand 2 is a 16-bit immediate
    HANDLE_OPERAND(immediate)
    HANDLE_OPERAND(immediate)
    break;
  case X86Local::MRM0X:
  case X86Local::MRM1X:
  case X86Local::MRM2X:
  case X86Local::MRM3X:
  case X86Local::MRM4X:
  case X86Local::MRM5X:
  case X86Local::MRM6X:
  case X86Local::MRM7X:
#define MAP(from, to) case X86Local::MRM_##from:
    X86_INSTR_MRM_MAPPING
#undef MAP
    HANDLE_OPTIONAL(relocation)
    break;
  }

#undef HANDLE_OPERAND
#undef HANDLE_OPTIONAL
}

void RecognizableInstr::emitDecodePath(DisassemblerTables &tables) const {
  // Special cases where the LLVM tables are not complete

#define MAP(from, to) case X86Local::MRM_##from:

  std::optional<OpcodeType> opcodeType;
  switch (OpMap) {
  default:
    llvm_unreachable("Invalid map!");
  case X86Local::OB:
    opcodeType = ONEBYTE;
    break;
  case X86Local::TB:
    opcodeType = TWOBYTE;
    break;
  case X86Local::T8:
    opcodeType = THREEBYTE_38;
    break;
  case X86Local::TA:
    opcodeType = THREEBYTE_3A;
    break;
  case X86Local::XOP8:
    opcodeType = XOP8_MAP;
    break;
  case X86Local::XOP9:
    opcodeType = XOP9_MAP;
    break;
  case X86Local::XOPA:
    opcodeType = XOPA_MAP;
    break;
  case X86Local::ThreeDNow:
    opcodeType = THREEDNOW_MAP;
    break;
  case X86Local::T_MAP4:
    opcodeType = MAP4;
    break;
  case X86Local::T_MAP5:
    opcodeType = MAP5;
    break;
  case X86Local::T_MAP6:
    opcodeType = MAP6;
    break;
  case X86Local::T_MAP7:
    opcodeType = MAP7;
    break;
  }

  std::unique_ptr<ModRMFilter> filter;
  switch (Form) {
  default:
    llvm_unreachable("Invalid form!");
  case X86Local::Pseudo:
    llvm_unreachable("Pseudo should not be emitted!");
  case X86Local::RawFrm:
  case X86Local::AddRegFrm:
  case X86Local::RawFrmMemOffs:
  case X86Local::RawFrmSrc:
  case X86Local::RawFrmDst:
  case X86Local::RawFrmDstSrc:
  case X86Local::RawFrmImm8:
  case X86Local::RawFrmImm16:
  case X86Local::AddCCFrm:
  case X86Local::PrefixByte:
    filter = std::make_unique<DumbFilter>();
    break;
  case X86Local::MRMDestReg:
  case X86Local::MRMDestRegCC:
  case X86Local::MRMSrcReg:
  case X86Local::MRMSrcReg4VOp3:
  case X86Local::MRMSrcRegOp4:
  case X86Local::MRMSrcRegCC:
  case X86Local::MRMXrCC:
  case X86Local::MRMXr:
    filter = std::make_unique<ModFilter>(true);
    break;
  case X86Local::MRMDestMem:
  case X86Local::MRMDestMemCC:
  case X86Local::MRMDestMem4VOp3CC:
  case X86Local::MRMDestMemFSIB:
  case X86Local::MRMSrcMem:
  case X86Local::MRMSrcMemFSIB:
  case X86Local::MRMSrcMem4VOp3:
  case X86Local::MRMSrcMemOp4:
  case X86Local::MRMSrcMemCC:
  case X86Local::MRMXmCC:
  case X86Local::MRMXm:
    filter = std::make_unique<ModFilter>(false);
    break;
  case X86Local::MRM0r:
  case X86Local::MRM1r:
  case X86Local::MRM2r:
  case X86Local::MRM3r:
  case X86Local::MRM4r:
  case X86Local::MRM5r:
  case X86Local::MRM6r:
  case X86Local::MRM7r:
    filter = std::make_unique<ExtendedFilter>(true, Form - X86Local::MRM0r);
    break;
  case X86Local::MRM0X:
  case X86Local::MRM1X:
  case X86Local::MRM2X:
  case X86Local::MRM3X:
  case X86Local::MRM4X:
  case X86Local::MRM5X:
  case X86Local::MRM6X:
  case X86Local::MRM7X:
    filter = std::make_unique<ExtendedFilter>(true, Form - X86Local::MRM0X);
    break;
  case X86Local::MRMr0:
    filter = std::make_unique<ExtendedRMFilter>(true, Form - X86Local::MRMr0);
    break;
  case X86Local::MRM0m:
  case X86Local::MRM1m:
  case X86Local::MRM2m:
  case X86Local::MRM3m:
  case X86Local::MRM4m:
  case X86Local::MRM5m:
  case X86Local::MRM6m:
  case X86Local::MRM7m:
    filter = std::make_unique<ExtendedFilter>(false, Form - X86Local::MRM0m);
    break;
    X86_INSTR_MRM_MAPPING
    filter = std::make_unique<ExactFilter>(0xC0 + Form - X86Local::MRM_C0);
    break;
  } // switch (Form)

  uint8_t opcodeToSet = Opcode;

  unsigned AddressSize = 0;
  switch (AdSize) {
  case X86Local::AdSize16:
    AddressSize = 16;
    break;
  case X86Local::AdSize32:
    AddressSize = 32;
    break;
  case X86Local::AdSize64:
    AddressSize = 64;
    break;
  }

  assert(opcodeType && "Opcode type not set");
  assert(filter && "Filter not set");

  if (Form == X86Local::AddRegFrm || Form == X86Local::MRMSrcRegCC ||
      Form == X86Local::MRMSrcMemCC || Form == X86Local::MRMXrCC ||
      Form == X86Local::MRMXmCC || Form == X86Local::AddCCFrm ||
      Form == X86Local::MRMDestRegCC || Form == X86Local::MRMDestMemCC ||
      Form == X86Local::MRMDestMem4VOp3CC) {
    uint8_t Count = Form == X86Local::AddRegFrm ? 8 : 16;
    assert(((opcodeToSet % Count) == 0) && "ADDREG_FRM opcode not aligned");

    uint8_t currentOpcode;

    for (currentOpcode = opcodeToSet;
         currentOpcode < (uint8_t)(opcodeToSet + Count); ++currentOpcode)
      tables.setTableFields(*opcodeType, insnContext(), currentOpcode, *filter,
                            UID, Is32Bit, OpPrefix == 0,
                            IgnoresVEX_L || EncodeRC, IgnoresW, AddressSize);
  } else {
    tables.setTableFields(*opcodeType, insnContext(), opcodeToSet, *filter, UID,
                          Is32Bit, OpPrefix == 0, IgnoresVEX_L || EncodeRC,
                          IgnoresW, AddressSize);
  }

#undef MAP
}

OperandType RecognizableInstr::typeFromString(StringRef Str, bool hasREX_W,
                                              uint8_t OpSize) {
  StringSwitch<OperandType> Switch(Str);
  if (hasREX_W) {
    // For instructions with a REX_W prefix, a declared 32-bit register encoding
    // is special.
    Switch.Case("GR32", TYPE_R32);
  }
  if (OpSize == X86Local::OpSize16) {
    // For OpSize16 instructions, a declared 16-bit register or
    // immediate encoding is special.
    Switch.Case("GR16", TYPE_Rv);
  } else if (OpSize == X86Local::OpSize32) {
    // For OpSize32 instructions, a declared 32-bit register or
    // immediate encoding is special.
    Switch.Case("GR32", TYPE_Rv);
  }
  // clang-format off
  OperandType Type =
      Switch.Case("i16mem", TYPE_M)
          .Case("i16imm", TYPE_IMM)
          .Case("i16i8imm", TYPE_IMM)
          .Case("GR16", TYPE_R16)
          .Case("GR16orGR32orGR64", TYPE_R16)
          .Case("i32mem", TYPE_M)
          .Case("i32imm", TYPE_IMM)
          .Case("i32i8imm", TYPE_IMM)
          .Case("GR32", TYPE_R32)
          .Case("GR32orGR64", TYPE_R32)
          .Case("i64mem", TYPE_M)
          .Case("i64i32imm", TYPE_IMM)
          .Case("i64i8imm", TYPE_IMM)
          .Case("GR64", TYPE_R64)
          .Case("i8mem", TYPE_M)
          .Case("i8imm", TYPE_IMM)
          .Case("u4imm", TYPE_UIMM8)
          .Case("u8imm", TYPE_UIMM8)
          .Case("i16u8imm", TYPE_UIMM8)
          .Case("i32u8imm", TYPE_UIMM8)
          .Case("i64u8imm", TYPE_UIMM8)
          .Case("GR8", TYPE_R8)
          .Case("VR128", TYPE_XMM)
          .Case("VR128X", TYPE_XMM)
          .Case("f128mem", TYPE_M)
          .Case("f256mem", TYPE_M)
          .Case("f512mem", TYPE_M)
          .Case("FR128", TYPE_XMM)
          .Case("FR64", TYPE_XMM)
          .Case("FR64X", TYPE_XMM)
          .Case("f64mem", TYPE_M)
          .Case("sdmem", TYPE_M)
          .Case("FR16X", TYPE_XMM)
          .Case("FR32", TYPE_XMM)
          .Case("FR32X", TYPE_XMM)
          .Case("f32mem", TYPE_M)
          .Case("f16mem", TYPE_M)
          .Case("ssmem", TYPE_M)
          .Case("shmem", TYPE_M)
          .Case("RST", TYPE_ST)
          .Case("RSTi", TYPE_ST)
          .Case("i128mem", TYPE_M)
          .Case("i256mem", TYPE_M)
          .Case("i512mem", TYPE_M)
          .Case("i512mem_GR16", TYPE_M)
          .Case("i512mem_GR32", TYPE_M)
          .Case("i512mem_GR64", TYPE_M)
          .Case("i64i32imm_brtarget", TYPE_REL)
          .Case("i8imm_brtarget", TYPE_REL)
          .Case("i16imm_brtarget", TYPE_REL)
          .Case("i32imm_brtarget", TYPE_REL)
          .Case("ccode", TYPE_IMM)
          .Case("cflags", TYPE_IMM)
          .Case("AVX512RC", TYPE_IMM)
          .Case("brtarget32", TYPE_REL)
          .Case("brtarget16", TYPE_REL)
          .Case("brtarget8", TYPE_REL)
          .Case("f80mem", TYPE_M)
          .Case("lea64_8mem", TYPE_M)
          .Case("lea64_16mem", TYPE_M)
          .Case("lea64_32mem", TYPE_M)
          .Case("lea64mem", TYPE_M)
          .Case("VR64", TYPE_MM64)
          .Case("i64imm", TYPE_IMM)
          .Case("anymem", TYPE_M)
          .Case("opaquemem", TYPE_M)
          .Case("sibmem", TYPE_MSIB)
          .Case("SEGMENT_REG", TYPE_SEGMENTREG)
          .Case("DEBUG_REG", TYPE_DEBUGREG)
          .Case("CONTROL_REG", TYPE_CONTROLREG)
          .Case("srcidx8", TYPE_SRCIDX)
          .Case("srcidx16", TYPE_SRCIDX)
          .Case("srcidx32", TYPE_SRCIDX)
          .Case("srcidx64", TYPE_SRCIDX)
          .Case("dstidx8", TYPE_DSTIDX)
          .Case("dstidx16", TYPE_DSTIDX)
          .Case("dstidx32", TYPE_DSTIDX)
          .Case("dstidx64", TYPE_DSTIDX)
          .Case("offset16_8", TYPE_MOFFS)
          .Case("offset16_16", TYPE_MOFFS)
          .Case("offset16_32", TYPE_MOFFS)
          .Case("offset32_8", TYPE_MOFFS)
          .Case("offset32_16", TYPE_MOFFS)
          .Case("offset32_32", TYPE_MOFFS)
          .Case("offset32_64", TYPE_MOFFS)
          .Case("offset64_8", TYPE_MOFFS)
          .Case("offset64_16", TYPE_MOFFS)
          .Case("offset64_32", TYPE_MOFFS)
          .Case("offset64_64", TYPE_MOFFS)
          .Case("VR256", TYPE_YMM)
          .Case("VR256X", TYPE_YMM)
          .Case("VR512", TYPE_ZMM)
          .Case("VK1", TYPE_VK)
          .Case("VK1WM", TYPE_VK)
          .Case("VK2", TYPE_VK)
          .Case("VK2WM", TYPE_VK)
          .Case("VK4", TYPE_VK)
          .Case("VK4WM", TYPE_VK)
          .Case("VK8", TYPE_VK)
          .Case("VK8WM", TYPE_VK)
          .Case("VK16", TYPE_VK)
          .Case("VK16WM", TYPE_VK)
          .Case("VK32", TYPE_VK)
          .Case("VK32WM", TYPE_VK)
          .Case("VK64", TYPE_VK)
          .Case("VK64WM", TYPE_VK)
          .Case("VK1Pair", TYPE_VK_PAIR)
          .Case("VK2Pair", TYPE_VK_PAIR)
          .Case("VK4Pair", TYPE_VK_PAIR)
          .Case("VK8Pair", TYPE_VK_PAIR)
          .Case("VK16Pair", TYPE_VK_PAIR)
          .Case("vx32mem", TYPE_MVSIBX)
          .Case("vx64mem", TYPE_MVSIBX)
          .Case("vy32mem", TYPE_MVSIBY)
          .Case("vy64mem", TYPE_MVSIBY)
          .Case("vx32xmem", TYPE_MVSIBX)
          .Case("vx64xmem", TYPE_MVSIBX)
          .Case("vy32xmem", TYPE_MVSIBY)
          .Case("vy64xmem", TYPE_MVSIBY)
          .Case("vz32mem", TYPE_MVSIBZ)
          .Case("vz64mem", TYPE_MVSIBZ)
          .Case("BNDR", TYPE_BNDR)
          .Case("TILE", TYPE_TMM)
          .Case("TILEPair", TYPE_TMM_PAIR)
          .Default(TYPE_NONE);
  // clang-format on

  if (Type != TYPE_NONE)
    return Type;
  errs() << "Unhandled type string " << Str << "\n";
  llvm_unreachable("Unhandled type string");
}

OperandEncoding RecognizableInstr::immediateEncodingFromString(StringRef Str,
                                                               uint8_t OpSize) {
  StringSwitch<OperandEncoding> Switch(Str);
  if (OpSize != X86Local::OpSize16) {
    // For instructions without an OpSize prefix, a declared 16-bit register or
    // immediate encoding is special.
    Switch.Case("i16imm", ENCODING_IW);
  }

  // clang-format off
  OperandEncoding Encoding =
      Switch.Case("i32i8imm", ENCODING_IB)
          .Case("AVX512RC", ENCODING_IRC)
          .Case("i16imm", ENCODING_Iv)
          .Case("i16i8imm", ENCODING_IB)
          .Case("i32imm", ENCODING_Iv)
          .Case("i64i32imm", ENCODING_ID)
          .Case("i64i8imm", ENCODING_IB)
          .Case("i8imm", ENCODING_IB)
          .Case("ccode", ENCODING_CC)
          .Case("cflags", ENCODING_CF)
          .Case("u4imm", ENCODING_IB)
          .Case("u8imm", ENCODING_IB)
          .Case("i16u8imm", ENCODING_IB)
          .Case("i32u8imm", ENCODING_IB)
          .Case("i64u8imm", ENCODING_IB)
          // This is not a typo.  Instructions like BLENDVPD put
          // register IDs in 8-bit immediates nowadays.
          .Case("FR32", ENCODING_IB)
          .Case("FR64", ENCODING_IB)
          .Case("FR128", ENCODING_IB)
          .Case("VR128", ENCODING_IB)
          .Case("VR256", ENCODING_IB)
          .Case("FR16X", ENCODING_IB)
          .Case("FR32X", ENCODING_IB)
          .Case("FR64X", ENCODING_IB)
          .Case("VR128X", ENCODING_IB)
          .Case("VR256X", ENCODING_IB)
          .Case("VR512", ENCODING_IB)
          .Case("TILE", ENCODING_IB)
          .Default(ENCODING_NONE);
  // clang-format on

  if (Encoding != ENCODING_NONE)
    return Encoding;
  errs() << "Unhandled immediate encoding " << Str << "\n";
  llvm_unreachable("Unhandled immediate encoding");
}

OperandEncoding
RecognizableInstr::rmRegisterEncodingFromString(StringRef Str, uint8_t OpSize) {
  // clang-format off
  auto Encoding =
      StringSwitch<OperandEncoding>(Str)
          .Case("RST", ENCODING_FP)
          .Case("RSTi", ENCODING_FP)
          .Case("GR16", ENCODING_RM)
          .Case("GR16orGR32orGR64", ENCODING_RM)
          .Case("GR32", ENCODING_RM)
          .Case("GR32orGR64", ENCODING_RM)
          .Case("GR64", ENCODING_RM)
          .Case("GR8", ENCODING_RM)
          .Case("VR128", ENCODING_RM)
          .Case("VR128X", ENCODING_RM)
          .Case("FR128", ENCODING_RM)
          .Case("FR64", ENCODING_RM)
          .Case("FR32", ENCODING_RM)
          .Case("FR64X", ENCODING_RM)
          .Case("FR32X", ENCODING_RM)
          .Case("FR16X", ENCODING_RM)
          .Case("VR64", ENCODING_RM)
          .Case("VR256", ENCODING_RM)
          .Case("VR256X", ENCODING_RM)
          .Case("VR512", ENCODING_RM)
          .Case("VK1", ENCODING_RM)
          .Case("VK2", ENCODING_RM)
          .Case("VK4", ENCODING_RM)
          .Case("VK8", ENCODING_RM)
          .Case("VK16", ENCODING_RM)
          .Case("VK32", ENCODING_RM)
          .Case("VK64", ENCODING_RM)
          .Case("BNDR", ENCODING_RM)
          .Case("TILE", ENCODING_RM)
          .Case("TILEPair", ENCODING_RM)
          .Default(ENCODING_NONE);
  // clang-format on
  if (Encoding != ENCODING_NONE)
    return Encoding;
  errs() << "Unhandled R/M register encoding " << Str << "\n";
  llvm_unreachable("Unhandled R/M register encoding");
}

OperandEncoding
RecognizableInstr::roRegisterEncodingFromString(StringRef Str, uint8_t OpSize) {
  // clang-format off
  auto Encoding =
      StringSwitch<OperandEncoding>(Str)
          .Case("GR16", ENCODING_REG)
          .Case("GR16orGR32orGR64", ENCODING_REG)
          .Case("GR32", ENCODING_REG)
          .Case("GR32orGR64", ENCODING_REG)
          .Case("GR64", ENCODING_REG)
          .Case("GR8", ENCODING_REG)
          .Case("VR128", ENCODING_REG)
          .Case("FR128", ENCODING_REG)
          .Case("FR64", ENCODING_REG)
          .Case("FR32", ENCODING_REG)
          .Case("VR64", ENCODING_REG)
          .Case("SEGMENT_REG", ENCODING_REG)
          .Case("DEBUG_REG", ENCODING_REG)
          .Case("CONTROL_REG", ENCODING_REG)
          .Case("VR256", ENCODING_REG)
          .Case("VR256X", ENCODING_REG)
          .Case("VR128X", ENCODING_REG)
          .Case("FR64X", ENCODING_REG)
          .Case("FR32X", ENCODING_REG)
          .Case("FR16X", ENCODING_REG)
          .Case("VR512", ENCODING_REG)
          .Case("VK1", ENCODING_REG)
          .Case("VK2", ENCODING_REG)
          .Case("VK4", ENCODING_REG)
          .Case("VK8", ENCODING_REG)
          .Case("VK16", ENCODING_REG)
          .Case("VK32", ENCODING_REG)
          .Case("VK64", ENCODING_REG)
          .Case("VK1Pair", ENCODING_REG)
          .Case("VK2Pair", ENCODING_REG)
          .Case("VK4Pair", ENCODING_REG)
          .Case("VK8Pair", ENCODING_REG)
          .Case("VK16Pair", ENCODING_REG)
          .Case("VK1WM", ENCODING_REG)
          .Case("VK2WM", ENCODING_REG)
          .Case("VK4WM", ENCODING_REG)
          .Case("VK8WM", ENCODING_REG)
          .Case("VK16WM", ENCODING_REG)
          .Case("VK32WM", ENCODING_REG)
          .Case("VK64WM", ENCODING_REG)
          .Case("BNDR", ENCODING_REG)
          .Case("TILE", ENCODING_REG)
          .Case("TILEPair", ENCODING_REG)
          .Default(ENCODING_NONE);
  // clang-format on

  if (Encoding != ENCODING_NONE)
    return Encoding;

  errs() << "Unhandled reg/opcode register encoding " << Str << "\n";
  llvm_unreachable("Unhandled reg/opcode register encoding");
}

OperandEncoding
RecognizableInstr::vvvvRegisterEncodingFromString(StringRef Str,
                                                  uint8_t OpSize) {
  // clang-format off
  auto Encoding =
      StringSwitch<OperandEncoding>(Str)
          .Case("GR8", ENCODING_VVVV)
          .Case("GR16", ENCODING_VVVV)
          .Case("GR32", ENCODING_VVVV)
          .Case("GR64", ENCODING_VVVV)
          .Case("FR32", ENCODING_VVVV)
          .Case("FR128", ENCODING_VVVV)
          .Case("FR64", ENCODING_VVVV)
          .Case("VR128", ENCODING_VVVV)
          .Case("VR256", ENCODING_VVVV)
          .Case("FR16X", ENCODING_VVVV)
          .Case("FR32X", ENCODING_VVVV)
          .Case("FR64X", ENCODING_VVVV)
          .Case("VR128X", ENCODING_VVVV)
          .Case("VR256X", ENCODING_VVVV)
          .Case("VR512", ENCODING_VVVV)
          .Case("VK1", ENCODING_VVVV)
          .Case("VK2", ENCODING_VVVV)
          .Case("VK4", ENCODING_VVVV)
          .Case("VK8", ENCODING_VVVV)
          .Case("VK16", ENCODING_VVVV)
          .Case("VK32", ENCODING_VVVV)
          .Case("VK64", ENCODING_VVVV)
          .Case("TILE", ENCODING_VVVV)
          .Case("TILEPair", ENCODING_VVVV)
          .Default(ENCODING_NONE);
  // clang-format on
  if (Encoding != ENCODING_NONE)
    return Encoding;

  errs() << "Unhandled VEX.vvvv register encoding " << Str << "\n";
  llvm_unreachable("Unhandled VEX.vvvv register encoding");
}

OperandEncoding
RecognizableInstr::writemaskRegisterEncodingFromString(StringRef Str,
                                                       uint8_t OpSize) {
  // clang-format off
  auto Encoding =
      StringSwitch<OperandEncoding>(Str)
          .Case("VK1WM", ENCODING_WRITEMASK)
          .Case("VK2WM", ENCODING_WRITEMASK)
          .Case("VK4WM", ENCODING_WRITEMASK)
          .Case("VK8WM", ENCODING_WRITEMASK)
          .Case("VK16WM", ENCODING_WRITEMASK)
          .Case("VK32WM", ENCODING_WRITEMASK)
          .Case("VK64WM", ENCODING_WRITEMASK)
          .Default(ENCODING_NONE);
  // clang-format on

  if (Encoding != ENCODING_NONE)
    return Encoding;

  errs() << "Unhandled mask register encoding " << Str << "\n";
  llvm_unreachable("Unhandled mask register encoding");
}

OperandEncoding RecognizableInstr::memoryEncodingFromString(StringRef Str,
                                                            uint8_t OpSize) {
  // clang-format off
  auto Encoding =
      StringSwitch<OperandEncoding>(Str)
          .Case("i16mem", ENCODING_RM)
          .Case("i32mem", ENCODING_RM)
          .Case("i64mem", ENCODING_RM)
          .Case("i8mem", ENCODING_RM)
          .Case("shmem", ENCODING_RM)
          .Case("ssmem", ENCODING_RM)
          .Case("sdmem", ENCODING_RM)
          .Case("f128mem", ENCODING_RM)
          .Case("f256mem", ENCODING_RM)
          .Case("f512mem", ENCODING_RM)
          .Case("f64mem", ENCODING_RM)
          .Case("f32mem", ENCODING_RM)
          .Case("f16mem", ENCODING_RM)
          .Case("i128mem", ENCODING_RM)
          .Case("i256mem", ENCODING_RM)
          .Case("i512mem", ENCODING_RM)
          .Case("i512mem_GR16", ENCODING_RM)
          .Case("i512mem_GR32", ENCODING_RM)
          .Case("i512mem_GR64", ENCODING_RM)
          .Case("f80mem", ENCODING_RM)
          .Case("lea64_8mem", ENCODING_RM)
          .Case("lea64_16mem", ENCODING_RM)
          .Case("lea64_32mem", ENCODING_RM)
          .Case("lea64mem", ENCODING_RM)
          .Case("anymem", ENCODING_RM)
          .Case("opaquemem", ENCODING_RM)
          .Case("sibmem", ENCODING_SIB)
          .Case("vx32mem", ENCODING_VSIB)
          .Case("vx64mem", ENCODING_VSIB)
          .Case("vy32mem", ENCODING_VSIB)
          .Case("vy64mem", ENCODING_VSIB)
          .Case("vx32xmem", ENCODING_VSIB)
          .Case("vx64xmem", ENCODING_VSIB)
          .Case("vy32xmem", ENCODING_VSIB)
          .Case("vy64xmem", ENCODING_VSIB)
          .Case("vz32mem", ENCODING_VSIB)
          .Case("vz64mem", ENCODING_VSIB)
          .Default(ENCODING_NONE);
  // clang-format on

  if (Encoding != ENCODING_NONE)
    return Encoding;

  errs() << "Unhandled memory encoding " << Str << "\n";
  llvm_unreachable("Unhandled memory encoding");
}

OperandEncoding
RecognizableInstr::relocationEncodingFromString(StringRef Str, uint8_t OpSize) {
  StringSwitch<OperandEncoding> Switch(Str);

  if (OpSize != X86Local::OpSize16) {
    // For instructions without an OpSize prefix, a declared 16-bit register or
    // immediate encoding is special.
    Switch.Case("i16imm", ENCODING_IW);
  }

  // clang-format off
  OperandEncoding Encoding =
      Switch.Case("i16imm", ENCODING_Iv)
          .Case("i16i8imm", ENCODING_IB)
          .Case("i32imm", ENCODING_Iv)
          .Case("i32i8imm", ENCODING_IB)
          .Case("i64i32imm", ENCODING_ID)
          .Case("i64i8imm", ENCODING_IB)
          .Case("i8imm", ENCODING_IB)
          .Case("u8imm", ENCODING_IB)
          .Case("i16u8imm", ENCODING_IB)
          .Case("i32u8imm", ENCODING_IB)
          .Case("i64u8imm", ENCODING_IB)
          .Case("i64i32imm_brtarget", ENCODING_ID)
          .Case("i16imm_brtarget", ENCODING_IW)
          .Case("i32imm_brtarget", ENCODING_ID)
          .Case("i8imm_brtarget", ENCODING_IB)
          .Case("brtarget32", ENCODING_ID)
          .Case("brtarget16", ENCODING_IW)
          .Case("brtarget8", ENCODING_IB)
          .Case("i64imm", ENCODING_IO)
          .Case("offset16_8", ENCODING_Ia)
          .Case("offset16_16", ENCODING_Ia)
          .Case("offset16_32", ENCODING_Ia)
          .Case("offset32_8", ENCODING_Ia)
          .Case("offset32_16", ENCODING_Ia)
          .Case("offset32_32", ENCODING_Ia)
          .Case("offset32_64", ENCODING_Ia)
          .Case("offset64_8", ENCODING_Ia)
          .Case("offset64_16", ENCODING_Ia)
          .Case("offset64_32", ENCODING_Ia)
          .Case("offset64_64", ENCODING_Ia)
          .Case("srcidx8", ENCODING_SI)
          .Case("srcidx16", ENCODING_SI)
          .Case("srcidx32", ENCODING_SI)
          .Case("srcidx64", ENCODING_SI)
          .Case("dstidx8", ENCODING_DI)
          .Case("dstidx16", ENCODING_DI)
          .Case("dstidx32", ENCODING_DI)
          .Case("dstidx64", ENCODING_DI)
          .Default(ENCODING_NONE);
  // clang-format on

  if (Encoding != ENCODING_NONE)
    return Encoding;

  errs() << "Unhandled relocation encoding " << Str << "\n";
  llvm_unreachable("Unhandled relocation encoding");
}

OperandEncoding
RecognizableInstr::opcodeModifierEncodingFromString(StringRef Str,
                                                    uint8_t OpSize) {
  // clang-format off
  auto Encoding =
      StringSwitch<OperandEncoding>(Str)
          .Case("GR32", ENCODING_Rv)
          .Case("GR64", ENCODING_RO)
          .Case("GR16", ENCODING_Rv)
          .Case("GR8", ENCODING_RB)
          .Case("ccode", ENCODING_CC)
          .Default(ENCODING_NONE);
  // clang-format on
  if (Encoding != ENCODING_NONE)
    return Encoding;

  errs() << "Unhandled opcode modifier encoding " << Str << "\n";
  llvm_unreachable("Unhandled opcode modifier encoding");
}
