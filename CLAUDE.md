# Herb Sutter's Zero-Overhead Deterministic Exceptions (`throws`) Implementation Analysis

This document provides a comprehensive analysis of the LLVM/Clang codebase for implementing Herb Sutter's `throws` exception specifier with discriminant-based error propagation.

---

## Overview

### Semantic Equivalence

**C++:**
```cpp
T foo() throws;              // Returns T on success, std::error on failure
```

**C:**
```c
void foo() fails{std::error}; // Returns void, std::error is error return type
```

**Key insight:** `T foo() throws` in C++ is semantically equivalent to `void foo() fails{std::error}` in C. The only difference is that C++ binds the success return type T to the function signature, while C binds only the error type.

### No Two-Phase Rollback — Key Difference from Traditional EH

**Critical distinction:** `throws/fails` uses normal return semantics, not exception unwinding:

| Aspect | Traditional C++ EH | throws/fails |
|--------|---------------------|--------------|
| **Mechanism** | `throw` → unwinder → landing pad | `return` → discriminant check → branch |
| **Destructor calls** | During phase-2 unwinding (async) | During normal scope exit (sync) |
| **Unwinding phases** | 2-phase: search then unwind | None — just return and propagate |
| **Landing pads** | Required | Not needed |
| **Personality function** | Required | Not needed |
| **Zero-cost?** | Zero-cost at call site, expensive when thrown | Zero-cost always (no EH tables) |

**Error propagation flow:**
```cpp
T foo() throws {          // marks itself as potentially failing
  A a;                    // destructor needed
  auto result = bar();    // bar() throws
  
  if (result.failed) {    // caller checks discriminant
    // ~A() called on normal scope exit
    // propagate error to caller: return result.error
    return propagate(result);
  }
  // success: ~A() called at end of scope
  return result.value;
}
```

### IR Representation

Both are represented in LLVM IR as:
```llvm
; Struct return with discriminant (fallback mechanism)
%throws_result = type { [sizeof(T_or_E) x i8], i1 }  ; union + failed flag

; Actual representation for T foo() throws:
define { T, i1 } @foo(...) #throws {  ; #throws attribute for backend optimization
  ; ...
}

; Actual representation for void foo() fails{E}:
define { E, i1 } @foo(...) #throws {  ; E is the error type, i1 is discriminant
  ; ...                                                 ; (void return means only error matters)
}
```

**With `throws` attribute:** Backend can optimize using target-specific mechanisms (carry flag, extra register).

**Without backend support:** Falls back to struct return via multiple registers.

### Implementation Strategy

1. **IR Representation:** `{ union{T, E}, bool failed }` with `throws` attribute — backend can optimize or fallback to struct
2. **C++ `throws`:** `T foo() throws` → IR: `{ T, i1 }` return (T is success value)
3. **C `fails{E}:** `void foo() fails{E}` → IR: `{ E, i1 }` return (E is error value, void means no success value)
4. **Destructor handling:** Normal scope-based cleanup (no landing pads, no unwinding)
5. **Error propagation:** Caller checks discriminant, calls destructors, returns error
6. **Backend Optimization:** 
   - With `throws` attribute + `supportThrowsCC()`: target-specific discriminant
   - Without: struct return via registers
7. **Target-Specific Mechanisms:**
   - **x86_64:** Carry flag (CF in EFLAGS) — T in RAX, error indicated by CF
   - **AArch64:** Carry flag (C in NZCV) — T in X0, error indicated by NZCV.C
   - **RISC-V:** Extra register a2 (X12) — T in a0, discriminant in a2
   - **LoongArch:** Extra register a2 (R6) — T in a0, discriminant in a2

---

## Part 1: The `noexcept` Template

The `noexcept` implementation is the primary template for `throws`.

### 1.1 Exception Specification Type Enum

**File:** `clang/include/clang/Basic/ExceptionSpecificationType.h:20-33`

```cpp
enum ExceptionSpecificationType {
  EST_None,             // no exception specification
  EST_DynamicNone,      // throw()
  EST_Dynamic,          // throw(T1, T2)
  EST_MSAny,            // Microsoft throw(...) extension
  EST_NoThrow,          // Microsoft __declspec(nothrow) extension
  EST_BasicNoexcept,    // noexcept
  EST_DependentNoexcept,// noexcept(expression), value-dependent
  EST_NoexceptFalse,    // noexcept(expression), evals to false
  EST_NoexceptTrue,     // noexcept(expression), evals to true
  EST_Unevaluated,      // not evaluated yet
  EST_Uninstantiated,   // not instantiated yet
  EST_Unparsed          // not parsed yet
};
```

**For `throws`: Add new values:**
- `EST_BasicThrows` — simple `throws`
- `EST_ThrowsTyped` — `throws(T)` with specific error type
- `EST_DependentThrows` — `throws(expr)` if conditional throws is supported

### 1.2 Parser Handling

**File:** `clang/lib/Parse/ParseDeclCXX.cpp:3925-4029`

Function: `Parser::tryParseExceptionSpecification()`

Parsing flow for `noexcept`:
- Line 3949: `noexcept` without argument → returns `EST_BasicNoexcept`
- Lines 3996-4006: `noexcept(expression)` → parses expression

**For `throws` (C++):**
1. Add `tok::kw_throws` in `clang/include/clang/Basic/TokenKinds.def`
2. Parse `throws` → `EST_BasicThrows`
3. Parse `throws(T)` → `EST_ThrowsTyped` with error type storage

**For `fails{E}` (C):**
1. Add `tok::kw_fails` token
2. Parse `fails{E}` → store error type E
3. C-specific parsing in `clang/lib/Parse/ParseDecl.c`

### 1.3 AST Storage — FunctionProtoType

**File:** `clang/include/clang/AST/TypeBase.h:5360-5749`

`FunctionProtoType` uses trailing objects:
- 4-bit field `FunctionTypeBits.ExceptionSpecType` (line 2013)
- For computed noexcept: stores `Expr*`
- For dynamic specs: stores `ArrayRef<QualType>` of exception types

**Key struct:** `ExceptionSpecInfo` (lines 5426-5449)

**For `throws`:**
- Use same 4-bit storage for `EST_BasicThrows`
- For `throws(T)` or `fails{E}`: store error type `QualType` in trailing objects
- Method: `getErrorType()` returns the declared error type

### 1.4 Semantic Analysis

**File:** `clang/lib/Sema/SemaExceptionSpec.cpp`

**File:** `clang/lib/AST/Type.cpp:3969-3999`

`FunctionProtoType::canThrow()`:
```cpp
switch (getExceptionSpecType()) {
  case EST_BasicNoexcept:
  case EST_NoexceptTrue:
    return CT_Cannot;
  case EST_NoexceptFalse:
    return CT_Can;
  case EST_DependentNoexcept:
    return CT_Dependent;
}
```

**For `throws`:**
- Add `CT_Deterministic` to `CanThrowResult` enum
- `EST_BasicThrows` / `EST_ThrowsTyped` → `CT_Deterministic`
- This signals codegen to use discriminant-based error handling

### 1.5 Code Generation — noexcept Reference

**File:** `clang/lib/CodeGen/CGCall.cpp:2017-2019`

```cpp
if (FPT->isNothrow())
  FuncAttrs.addAttribute(llvm::Attribute::NoUnwind);
```

**File:** `clang/lib/CodeGen/CGException.cpp:493-542`

For noexcept functions, terminate scope is pushed.

**For `throws`:**
- DO NOT add `nounwind`
- ADD `"throws"` or `"deterministic-eh"` attribute
- No terminate scope (errors return normally)

### 1.6 Name Mangling

**File:** `clang/lib/AST/ItaniumMangle.cpp:3712-3726`

Noexcept: `Do` suffix

**For `throws`:**
- Add `Dt` mangling for basic `throws`
- For `throws(T)` or `fails{E}`: mangle error type

---

## Part 2: IR Representation — Struct Fallback

### 2.1 StructType in LLVM IR

**File:** `llvm/include/llvm/IR/DerivedTypes.h:258-443`

`StructType` class represents structs:
- **Literal structs:** `{ i32, i32 }` — uniqued structurally
- **Identified structs:** `%foo` — can have names

### 2.2 Union Representation

Unions in LLVM IR are structs with overlapping fields — they share memory layout, size is largest member.

**Example IR for `throws(T)` return:**
```llvm
; Return type: { union{T, E}; bool; }
%throws_result = type { [sizeof(T_or_E) x i8], i1 }  ; Union + discriminant

define %throws_result @my_function(...) {
  ; ...
  ret %throws_result { ... }
}
```

**Better representation (using struct):**
```llvm
%throws_result_T = type { T, i1 }  ; Value + success flag
%throws_result_E = type { E, i1 }  ; Error + failure flag
```

### 2.3 ABIArgInfo — Argument/Return Classification

**File:** `clang/include/clang/CodeGen/CGFunctionInfo.h:32-93`

```cpp
class ABIArgInfo {
  enum Kind {
    Direct,       // Pass directly in register
    Extend,       // Extend to larger type
    Indirect,     // Pass indirectly (sret/byval)
    Ignore,       // Don't pass
    Expand,       // Expand into multiple args
    CoerceAndExpand,
    InAlloca      // In alloca (MSVC)
  };
};
```

**For `throws`:**
- Use `Direct` with coercion to `{T, bool}` struct
- Or `Expand` to multiple return registers
- Backend optimizes based on `"throws"` attribute

### 2.4 X86-64 Return Classification

**File:** `clang/lib/CodeGen/Targets/X86.cpp:2601-2732`

`X86_64ABIInfo::classifyReturnType()`:
- Structs ≤ 16 bytes: returned in registers (RAX+RDX or XMM0+XMM1)
- Structs > 64 bytes: sret (hidden pointer parameter)

**For `{T, bool}` struct:**
- If T ≤ 64-bit: RAX for T, RDX for bool
- This is the fallback mechanism when no carry flag support

### 2.5 Multiple Return Registers

**File:** `llvm/lib/Target/X86/X86CallingConv.td:239-256`

```tablegen
def RetCC_X86Common : CallingConv<[
  CCIfType<[i32], CCAssignToReg<[EAX, EDX, ECX]>>,
  CCIfType<[i64], CCAssignToReg<[RAX, RDX, RCX]>>,
]>;
```

**File:** `clang/lib/CodeGen/Targets/X86.cpp:2558-2599`

`GetX86_64ByValArgumentPair()` creates struct types for two-register returns.

### 2.6 SelectionDAG Return Lowering

**File:** `llvm/lib/CodeGen/SelectionDAG/SelectionDAGBuilder.cpp:2194-2320`

`visitRet()` handles return instruction lowering:
```cpp
if (!FuncInfo.CanLowerReturn) {
  // sret demotion handling
  Register DemoteReg = FuncInfo.DemoteRegister;
}
```

**File:** `llvm/lib/CodeGen/SelectionDAG/FunctionLoweringInfo.cpp:96-102`

```cpp
CanLowerReturn = TLI->CanLowerReturn(CC, *MF, Fn->isVarArg(), Outs, ...);
```

---

## Part 3: Target-Specific Discriminant Mechanisms

### 3.1 x86-64 — Carry Flag (CF)

#### EFLAGS Register

**File:** `llvm/lib/Target/X86/X86RegisterInfo.td:466`

```tablegen
def EFLAGS : X86Reg<"flags", 0>, DwarfRegNum<[49, 9, 9]>;
```

#### Condition Codes for Carry

**File:** `llvm/lib/Target/X86/MCTargetDesc/X86BaseInfo.h:77-103`

```cpp
enum CondCode {
  COND_B = 2,    // Below (CF=1) — Carry Flag set
  COND_AE = 3,   // Above or Equal (CF=0) — Carry Flag clear
  // ...
};
```

**Key:** `COND_B` tests CF=1, `COND_AE` tests CF=0.

#### Setting Carry Flag

**File:** `llvm/lib/Target/X86/X86InstrMisc.td:1022-1026`

```tablegen
def CLC : I<0xF8, RawFrm, (outs), (ins), "clc", []>;  // Clear carry
def STC : I<0xF9, RawFrm, (outs), (ins), "stc", []>;  // Set carry
def CMC : I<0xF5, RawFrm, (outs), (ins), "cmc", []>;  // Complement carry
```

**Alternative (used in overflow handling):**
**File:** `llvm/lib/Target/X86/X86ISelLowering.cpp:34069-34072`

```cpp
// ADD(Carry, AllOnes) sets CF if Carry is non-zero, clears if zero
Carry = DAG.getNode(X86ISD::ADD, DL, DAG.getVTList(CarryVT, MVT::i32),
                    Carry, DAG.getAllOnesConstant(DL, CarryVT));
```

#### Reading Carry Flag

**File:** `llvm/lib/Target/X86/X86ISelLowering.cpp:23617-23622`

```cpp
// SETCC with COND_B reads carry into register
SDValue getSETCC(X86::CondCode Cond, SDValue EFLAGS, const SDLoc &dl,
                 SelectionDAG &DAG) {
  return DAG.getNode(X86ISD::SETCC, dl, MVT::i8,
                     DAG.getTargetConstant(Cond, dl, MVT::i8), EFLAGS);
}
```

#### SETB_C Pseudo Instructions

**File:** `llvm/lib/Target/X86/X86InstrCompiler.td:378-386`

```tablegen
def SETB_C32r : I<0, Pseudo, (outs GR32:$dst), (ins), "", []>;
def SETB_C64r : I<0, Pseudo, (outs GR64:$dst), (ins), "", []>;
```

Expand to `SBB32rr/SBB64rr` — materializes carry as all-ones or all-zeros.

**File:** `llvm/lib/Target/X86/X86InstrInfo.cpp:6176-6179`

```cpp
case X86::SETB_C32r:
  return Expand2AddrUndef(MIB, get(X86::SBB32rr));
```

#### X86ISD Nodes for Flag Operations

**File:** `llvm/lib/Target/X86/X86InstrFragments.td:392-393`

```tablegen
def X86adc_flag  : SDNode<"X86ISD::ADC",  SDTBinaryArithWithFlagsInOut>;
def X86sbb_flag  : SDNode<"X86ISD::SBB",  SDTBinaryArithWithFlagsInOut>;
```

**File:** `llvm/lib/Target/X86/X86InstrFragments.td:202-204`

```tablegen
def X86setcc_c : SDNode<"X86ISD::SETCC_CARRY", SDTX86SetCC_C>;
```

#### ISD Opcodes for Carry

**File:** `llvm/include/llvm/CodeGen/ISDOpcodes.h:287-330`

```cpp
ADDC, SUBC,        // Carry-setting
ADDE, SUBE,        // Carry-using
UADDO_CARRY,       // Unsigned add with carry
USUBO_CARRY,       // Unsigned sub with carry
SETCCCARRY,        // SETCC with carry
```

#### Overflow Intrinsics Reference

**File:** `llvm/include/llvm/IR/Intrinsics.td:1657-1676`

```tablegen
def int_uadd_with_overflow : DefaultAttrsIntrinsic<
    [llvm_anyint_ty, LLVMScalarOrSameVectorWidth<0, llvm_i1_ty>],
    [LLVMMatchType<0>, LLVMMatchType<0>]>;
```

Returns `{result, overflow}` where overflow is carry flag.

#### x86-64 throws Implementation Strategy

**Caller side:**
```cpp
// After call, check carry flag
// jc error_label  // Jump if carry (error)
// jnc success_label  // Jump if no carry (success)
```

**Callee side:**
```cpp
// Before return:
// clc  // Clear carry for success
// stc  // Set carry for error
```

---

### 3.2 AArch64 — Carry Flag (C in NZCV)

#### NZCV Register

**File:** `llvm/lib/Target/AArch64/AArch64RegisterInfo.td:172`

```tablegen
def NZCV  : AArch64Reg<0, "nzcv">;
```

**File:** `llvm/lib/Target/AArch64/AArch64RegisterInfo.td:308-314`

```tablegen
def CCR : RegisterClass<"AArch64", [FlagsVT], 32, (add NZCV)> {
  let CopyCost = -1;  // Don't allow copying of status registers.
  let isAllocatable = 0;
}
```

#### Condition Codes

**File:** `llvm/lib/Target/AArch64/Utils/AArch64BaseInfo.h:288-313`

```cpp
enum CondCode {
  HS = 0x2,      // Unsigned higher or same (C==1) — Carry set
  LO = 0x3,      // Unsigned lower (C==0) — Carry clear
  HI = 0x8,      // Unsigned higher (C==1 && Z==0)
  LS = 0x9,      // Unsigned lower or same
  // ...
};
```

**NZCV flag bits:**
```cpp
enum { N = 8, Z = 4, C = 2, V = 1 };
case HS: return C; // C == 1 (Carry set)
case LO: return 0; // C == 0 (Carry clear)
```

#### CSET/CINC Instructions

**File:** `llvm/lib/Target/AArch64/AArch64InstrInfo.td:3572-3585`

```tablegen
// CSET is alias for CSINC with zero registers
def : InstAlias<"cset $dst, $cc",
                (CSINCWr GPR32:$dst, WZR, WZR, inv_ccode:$cc)>;
def : InstAlias<"cset $dst, $cc",
                (CSINCXr GPR64:$dst, XZR, XZR, inv_ccode:$cc)>;
```

**CSET pattern:** `csinc Rd, WZR, WZR, invert(cc)` returns 1 if condition true, 0 otherwise.

#### Reading Carry Flag

**File:** `llvm/lib/Target/AArch64/AArch64ISelLowering.cpp:4587-4596`

```cpp
static SDValue carryFlagToValue(SDValue Glue, EVT VT, SelectionDAG &DAG, bool Invert) {
  SDValue Zero = DAG.getConstant(0, DL, VT);
  SDValue One = DAG.getConstant(1, DL, VT);
  AArch64CC::CondCode Cond = Invert ? AArch64CC::LO : AArch64CC::HS;
  SDValue CC = getCondCode(DAG, Cond);
  return DAG.getNode(AArch64ISD::CSEL, DL, VT, One, Zero, CC, Glue);
}
```

#### AArch64ISD Nodes

**File:** `llvm/lib/Target/AArch64/AArch64InstrInfo.td:849-875`

```tablegen
def AArch64csel          : SDNode<"AArch64ISD::CSEL", SDT_AArch64CSel>;
def AArch64csinc         : SDNode<"AArch64ISD::CSINC", SDT_AArch64CSel>;
def AArch64add_flag      : SDNode<"AArch64ISD::ADDS", SDTBinaryArithWithFlagsOut>;
def AArch64sub_flag      : SDNode<"AArch64ISD::SUBS", SDTBinaryArithWithFlagsOut>;
def AArch64adc_flag      : SDNode<"AArch64ISD::ADCS", SDTBinaryArithWithFlagsInOut>;
def AArch64sbc_flag      : SDNode<"AArch64ISD::SBCS", SDTBinaryArithWithFlagsInOut>;
```

#### Inline Assembly Flag Output (Precedent)

**File:** `llvm/lib/Target/AArch64/AArch64ISelLowering.cpp:13531-13551`

```cpp
static AArch64CC::CondCode parseConstraintCode(llvm::StringRef Constraint) {
  return StringSwitch<AArch64CC::CondCode>(Constraint)
    .Case("{@cccs}",  AArch64CC::HS)  // Carry set (C==1)
    .Case("{@cclo}",  AArch64CC::LO)  // Carry clear (C==0)
    ...
}
```

#### AArch64 throws Implementation Strategy

**Caller side:**
```asm
bl function
cset w0, hs    ; Set w0=1 if carry (error), 0 if success
cbnz w0, error_handler
```

**Callee side:**
```asm
; Success path:
subs xzr, xzr, xzr  ; Clear carry (NZCV.C = 0, comparison 0==0)
ret

; Error path:
subs xzr, xzr, #1  ; Set carry (NZCV.C = 1, comparison 0 < 1)
ret
```

---

### 3.3 RISC-V — Extra Register (a2/X12)

#### Return Registers

**File:** `llvm/lib/Target/RISCV/RISCVCallingConv.cpp:167-178`

```cpp
// ArgIGPRs for returns: X10 (a0), X11 (a1), X12 (a2), ...
static const MCPhysReg ArgIGPRs[] = {
  RISCV::X10, RISCV::X11, RISCV::X12, RISCV::X13,
  RISCV::X14, RISCV::X15, RISCV::X16, RISCV::X17
};
```

**Return registers:**
- `a0` (X10): Primary return value
- `a1` (X11): Second return value (for structs)
- `a2` (X12): Available for discriminant

#### Return Limit

**File:** `llvm/lib/Target/RISCV/RISCVCallingConv.cpp:402-410`

```cpp
// Any return value split into more than two values can't be returned directly.
if ((!LocVT.isVector() || Subtarget.isPExtPackedType(LocVT)) && IsRet &&
    ValNo > 1)
  return true;  // Force sret
```

**For throws:** Modify to allow `ValNo == 2` (third value) when `throws` attribute present.

#### Floating-Point Returns

**File:** `llvm/lib/Target/RISCV/RISCVCallingConv.cpp:83-91`

```cpp
static const MCPhysReg ArgFPR32s[] = {
  RISCV::F10_F, RISCV::F11_F, RISCV::F12_F, ...
};
static const MCPhysReg ArgFPR64s[] = {
  RISCV::F10_D, RISCV::F11_D, RISCV::F12_D, ...
};
```

#### RISC-V throws Implementation Strategy

**Return convention:**
- `a0` (X10): Return value T
- `a1` (X11): Second part if T needs two registers
- `a2` (X12): Discriminant bool (0 = success, non-zero = error)

**Caller side:**
```asm
call function
beqz a2, success  ; Branch if discriminant is zero (success)
mv a0, a0         ; Error value in a0 (or a0+a1)
j error_handler
success:
  ; Normal value in a0 (or a0+a1)
```

**Callee side:**
```asm
li a2, 0          ; Success: set discriminant to 0
ret
; Or:
li a2, 1          ; Error: set discriminant to non-zero
ret
```

---

### 3.4 LoongArch — Extra Register (a2/R6)

#### Return Registers

**File:** `llvm/lib/Target/LoongArch/LoongArchISelLowering.cpp:8826-8832`

```cpp
// a0-a1 reused to return values
static const MCPhysReg ArgGPRs[] = {
  LoongArch::R4, LoongArch::R5, LoongArch::R6, LoongArch::R7,
  LoongArch::R8, LoongArch::R9, LoongArch::R10, LoongArch::R11
};
```

**Register aliases:**
**File:** `llvm/lib/Target/LoongArch/LoongArchRegisterInfo.td:64-65`

```tablegen
def R4  : LA64Reg<4, "a0">;  // Primary return
def R5  : LA64Reg<5, "a1">;  // Second return
def R6  : LA64Reg<6, "a2">;  // Available for discriminant
```

#### Return Limit

**File:** `llvm/lib/Target/LoongArch/LoongArchISelLowering.cpp:8931-8934`

```cpp
// Any return value split into more than two values can't be returned directly.
if (IsRet && ValNo > 1)
  return true;  // Force sret
```

**For throws:** Modify to allow `ValNo == 2` when `throws` attribute present.

#### LoongArch throws Implementation Strategy

**Return convention:**
- `a0` (R4): Return value T
- `a1` (R5): Second part if T needs two registers
- `a2` (R6): Discriminant bool (0 = success, non-zero = error)

---

## Part 4: LLVM Attribute System

### 4.1 Attribute Enum Definition

**File:** `llvm/include/llvm/IR/Attributes.h:124-132`

```cpp
enum AttrKind {
  None,
  #define GET_ATTR_ENUM
  #include "llvm/IR/Attributes.inc"
  EndAttrKinds,
};
```

**File:** `llvm/include/llvm/IR/Attributes.td` — TableGen definitions.

### 4.2 Adding New Attribute (throws)

**Step 1: Define in Attributes.td**
```tablegen
def Throws : EnumAttr<"throws", IntersectPreserve, [FnAttr]>;
```

**Step 2: Add to ArgFlagsTy**
**File:** `llvm/include/llvm/CodeGen/TargetCallingConv.h:27-198`

```cpp
struct ArgFlagsTy {
  unsigned IsThrows : 1;
  bool isThrows() const { return IsThrows; }
  void setThrows() { IsThrows = 1; }
};
```

**Step 3: Add predicate in TargetCallingConv.td**
**File:** `llvm/include/llvm/Target/TargetCallingConv.td:59-62`

```tablegen
class CCIfThrows<CCAction A> : CCIf<"ArgFlags.isThrows()", A> {}
```

### 4.3 AttributeList

**File:** `llvm/include/llvm/IR/Attributes.h:544-1085`

Indices:
- `AttributeList::ReturnIndex = 0U`
- `AttributeList::FunctionIndex = ~0U`
- `AttributeList::FirstArgIndex = 1`

### 4.4 Swift Error Handling Reference

**File:** `llvm/include/llvm/IR/Attributes.td:369-376`

```tablegen
def SwiftError : EnumAttr<"swifterror", IntersectPreserve, [ParamAttr]>;
def SwiftSelf : EnumAttr<"swiftself", IntersectPreserve, [ParamAttr]>;
```

**Key insight:** `swifterror` uses hidden parameter for error, similar pattern for `throws`.

**File:** `llvm/include/llvm/CodeGen/SwiftErrorValueTracking.h`

`SwiftErrorValueTracking` class promotes swifterror from memory to virtual registers.

### 4.5 Target Support Hook

**File:** `llvm/include/llvm/CodeGen/TargetLowering.h:4697-4701`

```cpp
virtual bool supportSwiftError() const { return false; }
```

**For throws:**
```cpp
virtual bool supportThrowsCC() const { return false; }
```

Targets override to indicate discriminant support:
- X86: `return true;` (carry flag)
- AArch64: `return true;` (NZCV.C)
- RISC-V: `return true;` (extra register)
- LoongArch: `return true;` (extra register)

---

## Part 5: Calling Convention Infrastructure

### 5.1 CallingConv Enum

**File:** `llvm/include/llvm/IR/CallingConv.h:21-302`

```cpp
namespace CallingConv {
  using ID = unsigned;
  enum {
    C = 0,
    Fast = 8,
    Cold = 9,
    Swift = 16,
    SwiftTail = 20,
    FirstTargetCC = 64,
    MaxID = 1023
  };
}
```

**Option:** Add new CC `Throws = 21` or use attribute-based approach.

### 5.2 CCValAssign

**File:** `llvm/include/llvm/CodeGen/CallingConvLower.h:34-143`

```cpp
class CCValAssign {
  enum LocInfo {
    Full, SExt, ZExt, AExt, BCvt, Trunc, VExt, FPExt, Indirect
  };
  bool isRegLoc() const;
  bool isMemLoc() const;
};
```

### 5.3 CCState

**File:** `llvm/include/llvm/CodeGen/CallingConvLower.h:171-552`

```cpp
class CCState {
  CallingConv::ID CC;
  bool IsVarArg;
  void AnalyzeReturn(...);
  void AnalyzeCallResult(...);
};
```

### 5.4 Target Calling Convention Handling

**X86 TableGen:**
**File:** `llvm/lib/Target/X86/X86CallingConv.td`

```tablegen
def RetCC_X86Common : CallingConv<[
  CCIfType<[i64], CCAssignToReg<[RAX, RDX, RCX]>>,
]>;
```

**For throws (X86-64):**
```tablegen
CCIfThrows<CCIfType<[i64], CCAssignToRegWithFlag<[RAX], CarryFlag]>>,
```

Or simpler: keep RAX for value, use carry flag implicitly.

---

## Part 6: Exception Handling Changes

### 6.1 Current EH Flow (invoke/landingpad)

**File:** `llvm/docs/LangRef.rst:10230-10329`

```
invoke @func(args) to label %normal unwind label %exception
```

**File:** `clang/lib/CodeGen/CGCall.cpp:6104-6166`

```cpp
bool CannotThrow = Attrs.hasFnAttr(llvm::Attribute::NoUnwind);
if (!CannotThrow && getInvokeDest())
  CI = Builder.CreateInvoke(...);
else
  CI = Builder.CreateCall(...);
```

**For throws:**
- Always use `call` (no `invoke`)
- Check discriminant after call
- No landing pad needed

### 6.2 Personality Functions

**File:** `clang/lib/CodeGen/CGException.cpp:97-131`

- `__gxx_personality_v0` — Itanium C++
- `__CxxFrameHandler3` — MSVC

**For throws:** No personality function needed.

### 6.3 Throw Expression

**File:** `clang/lib/CodeGen/ItaniumCXXABI.cpp:1460-1518`

Current: `__cxa_allocate_exception`, `__cxa_throw`

**For throws:** Throw becomes error return:
```cpp
// Set discriminant to error value
Builder.CreateStore(ErrorValue, DiscriminantSlot);
Builder.CreateRet(...);
```

### 6.4 Catch Handling

**File:** `clang/lib/CodeGen/ItaniumCXXABI.cpp:4945-4962`

Current: `__cxa_begin_catch`, `__cxa_end_catch`

**For throws:**
- `try` becomes discriminant branch
- `catch` becomes error handler block
- No runtime calls

### 6.5 Destructor Handling — No Two-Phase Rollback

**Key difference from traditional C++ EH:**

| Mechanism | Traditional EH | throws/fails |
|-----------|----------------|--------------|
| **Unwinding** | 2-phase: search → unwind | Normal return path |
| **Destructor timing** | During phase-2 unwinding | During normal scope exit |
| **Landing pads** | Required for cleanup | Not needed |
| **Personality function** | Required for unwinder | Not needed |
| **Control flow** | Asynchronous (throw jumps) | Synchronous (return + branch) |

**Traditional C++ EH (2-phase):**
```cpp
void foo() {
  A a;               // destructor needed
  bar();             // might throw
}
// If bar() throws:
// Phase 1: Search for catch handler
// Phase 2: Unwind stack, run ~A() via landing pad
```

**throws/fails (normal return):**
```cpp
void foo() throws {              // marks itself as potentially failing
  A a;                           // destructor needed
  bar();                         // if bar() throws/fails
  // bar returns with discriminant
  // foo checks discriminant
  // if error: ~A() called on normal scope exit, then return error
}
```

**IR representation for destructor cleanup:**
```llvm
define { void, i1 } @foo(...) #throws {
  ; construct A
  call void @A_constructor(%A* %a)
  
  ; call bar
  %result = call { void, i1 } @bar(...) #throws
  
  ; check discriminant
  %failed = extractvalue { void, i1 } %result, 1
  br i1 %failed, label %error_exit, label %normal_exit
  
normal_exit:
  ; continue normal execution
  br %return
  
error_exit:
  ; call destructor on normal path (not landing pad!)
  call void @A_destructor(%A* %a)
  br %propagate_error
  
propagate_error:
  ; return error to caller
  ret { void, i1 } { undef, i1 1 }
}
```

### 6.6 Error Propagation Pattern

**Caller code pattern:**
```cpp
T foo() throws {
  A a;
  B b;
  
  auto result = bar();  // bar() throws
  
  if (result.failed) {
    // ~b() called first (reverse construction order)
    // ~a() called next
    // propagate error: return result.error
    return propagate(result.error);
  }
  
  // success path
  // ~b() and ~a() called at normal end of scope
  return result.value;
}
```

**Generated IR structure:**
```llvm
define { T, i1 } @foo(...) #throws {
entry:
  ; construct objects
  call void @A_ctor(%A* %a)
  call void @B_ctor(%B* %b)
  
  ; call throws function
  %bar_result = call { X, i1 } @bar(...) #throws
  %bar_failed = extractvalue { X, i1 } %bar_result, 1
  
  ; branch on discriminant
  br i1 %bar_failed, label %cleanup_error, label %success
  
success:
  ; normal path
  call void @B_dtor(%B* %b)   ; end-of-scope destructors
  call void @A_dtor(%A* %a)
  ret { T, i1 } { %success_value, i1 0 }
  
cleanup_error:
  ; error propagation path (NOT landing pad!)
  call void @B_dtor(%B* %b)   ; reverse-order destructors
  call void @A_dtor(%A* %a)
  ret { T, i1 } { %error_value, i1 1 }
}
```

### 6.7 Key Implementation Implications

1. **No EH infrastructure needed:**
   - No `invoke` instructions
   - No landing pads
   - No personality function
   - No LSDA tables

2. **Normal code flow:**
   - All cleanup happens on normal return paths
   - Branch on discriminant determines which path
   - Destructors called explicitly before returning error

3. **Clang CodeGen changes:**
   - `EHScopeStack` not needed for throws functions
   - Cleanups become normal scope-based (like `goto` cleanup)
   - No `pushTerminate()` for throws (unlike noexcept)

4. **Function marking:**
   - Function declares `throws`/`fails` to indicate potential failure
   - Caller must check discriminant
   - Caller propagates error by returning with discriminant set

---

## Part 7: C Language `fails{E}` Syntax

### 7.1 Semantic Equivalence with C++ `throws`

**Key insight:**
```
C++: T foo() throws;         ≡  C: void foo() fails{std::error}  (with implicit T return)
C++: void foo() throws;      ≡  C: void foo() fails{std::error}  (no success value)
```

**IR representation (identical for both):**
```llvm
; T foo() throws in C++:
define { T, i1 } @foo(...) #throws {
  ; Returns {value, failed_flag}
}

; void foo() fails{E} in C:
define { E, i1 } @foo(...) #throws {
  ; Returns {error_value, failed_flag}
  ; When failed_flag=0: function succeeded (void return)
  ; When failed_flag=1: error_value contains E
}
```

**The only difference:**
- C++ binds the success return type `T` to the function signature
- C binds only the error type `E` to `fails{E}`
- Both use the same `throws` attribute in IR for backend optimization

### 7.2 Token Addition

**File:** `clang/include/clang/Basic/TokenKinds.def`

Add:
```
KEYWORD(fails, KEYC)
```

### 7.3 Parser Handling (C-specific)

**File:** `clang/lib/Parse/ParseDecl.c`

Parse `fails{E}` after function signature:
```cpp
// After parsing function signature
if (Tok.is(tok::kw_fails)) {
  ConsumeToken();
  if (Tok.is(tok::l_brace)) {
    ConsumeBrace();
    // Parse error type E
    QualType ErrorType = ParseTypeName();
    ExpectAndConsume(tok::r_brace);
    // Store in FunctionDecl
    // If function returns void, IR will be {E, i1}
    // If function returns T, IR will be {union{T,E}, i1}
  }
}
```

### 7.4 AST Storage for C

Similar to `throws` in C++:
- Store error type `QualType` in trailing objects
- Method: `getErrorType()` on `FunctionDecl` or `FunctionProtoType`
- Function return type remains as declared (void or T)

### 7.5 C Error Type Semantics

**C `fails{E}` behavior:**
- Allows any type E as error (not restricted to `std::error`)
- Error type is explicitly specified in `fails{E}` braces
- For `void foo() fails{E}`: IR returns `{E, i1}`
- For `T foo() fails{E}`: IR returns `{union{T, E}, i1}`

**Comparison with C++:**
| Feature | C++ `throws` | C `fails{E}` |
|---------|--------------|--------------|
| Success return type | T (explicit) | void (implicit) or T (explicit) |
| Error type | `std::error` (implicit) | E (explicit in braces) |
| IR representation | `{T, i1}` | `{E, i1}` or `{union{T,E}, i1}` |
| Attribute | `#throws` | `#throws` (same) |

---

## Part 8: Implementation Phases

### Phase 1: Clang Frontend (Parsing & AST)

| File | Change |
|------|--------|
| `clang/include/clang/Basic/TokenKinds.def` | Add `kw_throws`, `kw_fails` |
| `clang/include/clang/Basic/ExceptionSpecificationType.h` | Add `EST_BasicThrows`, `EST_ThrowsTyped` |
| `clang/lib/Parse/ParseDeclCXX.cpp` | Parse `throws` |
| `clang/lib/Parse/ParseDecl.c` | Parse `fails{E}` |
| `clang/include/clang/AST/TypeBase.h` | Store error type in FunctionProtoType |
| `clang/lib/Sema/SemaExceptionSpec.cpp` | Add `ActOnThrowsSpec()` |
| `clang/lib/AST/Type.cpp` | Add `CT_Deterministic` to `canThrow()` |
| `clang/lib/AST/TypePrinter.cpp` | Print `throws`/`fails{E}` |

### Phase 2: LLVM IR Attribute

| File | Change |
|------|--------|
| `llvm/include/llvm/IR/Attributes.td` | Add `Throws` attribute |
| `llvm/include/llvm/CodeGen/TargetCallingConv.h` | Add `IsThrows` flag |
| `llvm/include/llvm/Target/TargetCallingConv.td` | Add `CCIfThrows` predicate |
| `llvm/include/llvm/CodeGen/TargetLowering.h` | Add `supportThrowsCC()` hook |

### Phase 3: Code Generation

| File | Change |
|------|--------|
| `clang/lib/CodeGen/CGCall.cpp` | Add `"throws"` attribute |
| `clang/lib/CodeGen/CGException.cpp` | Skip EH for throws |
| `clang/lib/CodeGen/ItaniumCXXABI.cpp` | Throw → return |
| `clang/lib/CodeGen/CGFunctionInfo.h` | Add HasThrows info |

### Phase 4: Backend — Struct Fallback (Default)

| File | Change |
|------|--------|
| `clang/lib/CodeGen/Targets/X86.cpp` | Classify `{T, bool}` return |
| `llvm/lib/CodeGen/SelectionDAG/SelectionDAGBuilder.cpp` | Lower throws return |

### Phase 5: Backend — X86 Carry Flag

| File | Change |
|------|--------|
| `llvm/lib/Target/X86/X86CallingConv.td` | Throws return convention |
| `llvm/lib/Target/X86/X86ISelLowering.cpp` | Lower throws return with carry |
| `llvm/lib/Target/X86/X86ISelLoweringCall.cpp` | Lower throws call |

### Phase 6: Backend — AArch64 Carry Flag

| File | Change |
|------|--------|
| `llvm/lib/Target/AArch64/AArch64CallingConv.td` | Throws return convention |
| `llvm/lib/Target/AArch64/AArch64ISelLowering.cpp` | Lower throws with NZCV.C |

### Phase 7: Backend — RISC-V Extra Register

| File | Change |
|------|--------|
| `llvm/lib/Target/RISCV/RISCVCallingConv.cpp` | Allow 3-register return for throws |
| `llvm/lib/Target/RISCV/RISCVISelLowering.cpp` | Lower throws return with a2 |

### Phase 8: Backend — LoongArch Extra Register

| File | Change |
|------|--------|
| `llvm/lib/Target/LoongArch/LoongArchISelLowering.cpp` | Allow 3-register return, use R6 |

### Phase 9: Mangling & ABI

| File | Change |
|------|--------|
| `clang/lib/AST/ItaniumMangle.cpp` | Add `Dt` mangling |
| `clang/lib/AST/MicrosoftMangle.cpp` | Windows mangling |

---

## Part 9: Discriminant Mechanism Summary

### IR Representation with `throws` Attribute

**C++ `T foo() throws`:**
```llvm
define { T, i1 } @foo(...) #throws {
  ; i1 = 0: success, T is valid
  ; i1 = 1: failure, error indicated (T may be error value)
}
```

**C `void foo() fails{E}`:**
```llvm
define { E, i1 } @foo(...) #throws {
  ; i1 = 0: success (void return)
  ; i1 = 1: failure, E contains error
}
```

**C `T foo() fails{E}`:**
```llvm
define { [max(T,E) x i8], i1 } @foo(...) #throws {
  ; union + discriminant
  ; i1 = 0: union contains T
  ; i1 = 1: union contains E
}
```

### Fallback: Struct Return (No Backend Support)

**Calling convention when `supportThrowsCC() == false`:**
- `{T, i1}`: T in first register (RAX/a0), discriminant in second (RDX/a1)
- `{E, i1}`: E in first register, discriminant in second
- `{union, i1}`: union split across registers, discriminant in extra register

### Target-Specific Optimizations (Backend Support)

When `supportThrowsCC() == true`, backend uses optimized discriminant:

| Target | Mechanism | Caller Check | Callee Set |
|--------|-----------|--------------|------------|
| x86-64 | Carry flag (CF) | `jc error` / `jnc success` | `stc` (error) / `clc` (success) |
| AArch64 | NZCV.C flag | `cset w0, hs` (error check) | `subs xzr, xzr, #1` (set C) |
| RISC-V | Register a2 (X12) | `beqz a2, success` | `li a2, 0` (success) / `li a2, 1` (error) |
| LoongArch | Register a2 (R6) | `beqz $a2, success` | `li $a2, 0` (success) / `li $a2, 1` (error) |

### Discriminant Value Encoding

**Success:** Discriminant = 0 (CF=0, a2=0)
**Error:** Discriminant ≠ 0 (CF=1, a2≠0)

**For C++ `throws`:**
- Success: T in return register, discriminant = 0
- Error: error value in return register, discriminant ≠ 0

**For C `fails{E}`:**
- Success: no value (void), discriminant = 0
- Error: E in return register, discriminant ≠ 0

---

## Part 10: Key File Reference

### Clang Files

| Category | File | Key Lines |
|----------|------|-----------|
| **Parsing** | `clang/lib/Parse/ParseDeclCXX.cpp` | 3925-4029 |
| | `clang/lib/Parse/ParseDecl.c` | — |
| | `clang/include/clang/Basic/TokenKinds.def` | — |
| **AST/Types** | `clang/include/clang/AST/TypeBase.h` | 4565-5749, 2013 |
| | `clang/lib/AST/Type.cpp` | 3969-3999 |
| **Exception Spec** | `clang/include/clang/Basic/ExceptionSpecificationType.h` | 20-56 |
| **Sema** | `clang/lib/Sema/SemaExceptionSpec.cpp` | 85-116, 1113-1250 |
| **CodeGen** | `clang/lib/CodeGen/CGCall.cpp` | 2017-2019, 2553-2839, 6104-6166 |
| | `clang/lib/CodeGen/CGException.cpp` | 493-542, 778-826 |
| | `clang/lib/CodeGen/ItaniumCXXABI.cpp` | 1460-1518, 4945-5276 |
| | `clang/lib/CodeGen/CGFunctionInfo.h` | 32-93, 596-788 |
| | `clang/lib/CodeGen/Targets/X86.cpp` | 2601-2732, 2048-2051 |
| **Mangling** | `clang/lib/AST/ItaniumMangle.cpp` | 3712-3726 |
| **Printing** | `clang/lib/AST/TypePrinter.cpp` | 991-1089 |

### LLVM Files

| Category | File | Key Lines |
|----------|------|-----------|
| **Attributes** | `llvm/include/llvm/IR/Attributes.td` | 369-376 (swifterror ref) |
| | `llvm/include/llvm/IR/Attributes.h` | 124-132, 544-1085 |
| **Calling Conv** | `llvm/include/llvm/IR/CallingConv.h` | 21-302 |
| | `llvm/include/llvm/CodeGen/CallingConvLower.h` | 34-143, 171-552 |
| | `llvm/include/llvm/CodeGen/TargetCallingConv.h` | 27-198 |
| | `llvm/include/llvm/Target/TargetCallingConv.td` | 59-62 |
| **TargetLowering** | `llvm/include/llvm/CodeGen/TargetLowering.h` | 4697-4701, 4800-5127 |
| **SelectionDAG** | `llvm/lib/CodeGen/SelectionDAG/SelectionDAGBuilder.cpp` | 2194-2320, 500-611 |
| | `llvm/lib/CodeGen/FunctionLoweringInfo.cpp` | 96-102 |
| **X86** | `llvm/lib/Target/X86/X86CallingConv.td` | 239-256, 509-516 |
| | `llvm/lib/Target/X86/X86ISelLowering.cpp` | 23617-23622, 34069-34072 |
| | `llvm/lib/Target/X86/X86ISelLoweringCall.cpp` | 669-676, 744-950 |
| | `llvm/lib/Target/X86/X86RegisterInfo.td` | 466 (EFLAGS) |
| | `llvm/lib/Target/X86/MCTargetDesc/X86BaseInfo.h` | 77-103 (CondCode) |
| | `llvm/lib/Target/X86/X86InstrMisc.td` | 1022-1026 (CLC/STC) |
| | `llvm/lib/Target/X86/X86InstrCompiler.td` | 378-386 (SETB_C) |
| **AArch64** | `llvm/lib/Target/AArch64/Utils/AArch64BaseInfo.h` | 288-313 (CondCode) |
| | `llvm/lib/Target/AArch64/AArch64RegisterInfo.td` | 172 (NZCV), 308-314 (CCR) |
| | `llvm/lib/Target/AArch64/AArch64ISelLowering.cpp` | 4587-4596, 13531-13551 |
| | `llvm/lib/Target/AArch64/AArch64InstrInfo.td` | 3572-3585, 849-875 |
| **RISC-V** | `llvm/lib/Target/RISCV/RISCVCallingConv.cpp` | 83-91, 167-178, 402-410 |
| | `llvm/lib/Target/RISCV/RISCVRegisterInfo.td` | 200-201 (X10/X11) |
| | `llvm/lib/Target/RISCV/RISCVISelLowering.cpp` | 25342-25369 |
| **LoongArch** | `llvm/lib/Target/LoongArch/LoongArchISelLowering.cpp` | 8826-8832, 8931-8934, 10022-10038 |
| | `llvm/lib/Target/LoongArch/LoongArchRegisterInfo.td` | 64-65 (R4/R5) |
| **ISD Opcodes** | `llvm/include/llvm/CodeGen/ISDOpcodes.h` | 287-330 |
| **Intrinsics** | `llvm/include/llvm/IR/Intrinsics.td` | 1657-1676 (overflow ref) |
| **SwiftError Ref** | `llvm/include/llvm/CodeGen/SwiftErrorValueTracking.h` | — |

---

## Part 11: Open Technical Questions

### 1. Discriminant for Typed Errors

For `T foo() fails{E}` (C with both return and error types):
- IR: `{ [max(T,E) x i8], i1 }` — union representation
- Backend optimization: same discriminant mechanism, union in return register(s)
- When discriminant = 0: union contains T
- When discriminant ≠ 0: union contains E

### 2. Error Type Matching

- `catch(E e)` needs to match error type
- For C++ `throws`: implicit `std::error` matching
- For C `fails{E}`: explicit E type matching
- Discriminant + error value comparison

### 3. Interaction with Existing EH

- Can `throws` and traditional `throw` coexist in same program?
- What happens when `throws` function calls non-`throws` throwing function?
- Mixed mode handling: `throws` caller must handle traditional exceptions via try/catch

### 4. C `fails{E}` vs C++ `throws` — Resolved

**Semantic equivalence:**
```
T foo() throws (C++)     ≡ void foo() fails{std::error} (C)
void foo() throws (C++)  ≡ void foo() fails{std::error} (C)
```

**Difference:**
- C++ binds T (success return) to signature, uses implicit `std::error`
- C binds E (error type) to `fails{E}`, allows explicit error type
- IR uses same `throws` attribute, different struct composition

### 5. Backend Support Detection

- `supportThrowsCC()` returns true → use target-specific discriminant
- `supportThrowsCC()` returns false → use struct fallback
- Need per-target implementation: x86-64, AArch64, RISC-V, LoongArch

### 6. Optimization

- Dead discriminant elimination for known-success paths
- Inlining across throws boundaries
- SROA for struct fallback representation
- Carry flag propagation for flag-based targets

---

## Appendix: Related Work

1. **P0709R4: Zero-overhead deterministic exceptions** — Herb Sutter's proposal
2. **P0779R0: Propagating exceptions to callers** — Alternative approach
3. **Swift error handling** — `swifterror` attribute precedent
4. **Go error handling** — Return value for errors
5. **Rust Result<T, E>** — Type-based error propagation
6. **LLVM overflow intrinsics** — `{result, overflow}` pattern

---

This document serves as a comprehensive reference for implementing Herb Sutter's zero-overhead deterministic exceptions with target-specific discriminant mechanisms and struct fallback.