# Herb Sutter's Zero-Overhead Deterministic Exceptions (`throws`) Implementation Analysis

This document provides a comprehensive analysis of the LLVM/Clang codebase for implementing Herb Sutter's `throws` exception specifier with discriminant-based error propagation.

---

## Overview

### Semantic Equivalence

**C++:**
```cpp
T foo() throws;              // Returns T on success, std::error on failure (implicit error type)
T foo() fails{E};            // Returns T on success, E on failure (explicit error type)
```

**C:**
```c
T foo() fails{E};            // Same as C++ fails{E} — explicit error type required
```

**Key insight:** `throws` is C++-only with implicit `std::error`. `fails{E}` works in both C++ and C with explicit error type. Both require explicit handling with `try` or `catch fails`.

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
T foo() fails{E} {              // marks itself as potentially failing
  A a;                          // destructor needed
  either<T, E> result = catch fails(bar());  // bar() fails{E}
  
  if (!result.positive) {       // check .positive flag
    // ~A() called on normal scope exit
    return failure(result.right);  // propagate error
  }
  // success: ~A() called at end of scope
  return result.left;
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

1. **IR Representation:** `{ T, i1 }` with `throws` attribute — backend can optimize or fallback to struct
2. **C++ `throws`:** `T foo() throws` → implicit `std::error`, requires `try` handling
3. **`fails{E}` (C++ and C):** `T foo() fails{E}` → explicit error type E, **requires** `try` or `catch fails`
4. **Explicit handling:** Calling `fails{E}` function without `try`/`catch fails` wrapper = compile error
5. **`try(expr)`:** Auto-propagate error on failure, continue on success
6. **`catch fails(expr)`:** Returns `either{T, E}` with `.positive`, `.left`, `.right`
7. **`try expr;`:** Auto-propagate at statement level (C++ only, no parens)
8. **`catch fails(E e)`:** Catch block for explicit error handling
9. **Destructor handling:** Normal scope-based cleanup (no landing pads, no unwinding)
10. **Backend Optimization:** 
    - With `throws` attribute + `supportThrowsCC()`: target-specific discriminant
    - Without: struct return via registers
11. **Target-Specific Mechanisms:**
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

### 1.6 Name Mangling — Required for ABI Safety

**File:** `clang/lib/AST/ItaniumMangle.cpp`

**Why different mangling is required:**
```cpp
// Different ABI — must have different mangled names
T foo();            // Returns T in register(s), no discriminant
T foo() throws;     // Returns {T, i1} or T + carry flag

// If same mangling: linker could mismatch them → ABI corruption at runtime
```

**Noexcept mangling (existing):**
- `Do` suffix for `noexcept`

**Throws mangling (new):**
```cpp
// Basic throws
void func() throws;     // Mangled with throws indicator

// fails{E} — error type also mangled
int func() fails{int};  // Error type E part of mangling
```

**Mangling prevents:**
- Linking `throws` function to non-`throws` pointer
- ABI mismatch at runtime
- Calling convention confusion

**Implementation:**
- `throws` is part of canonical type (affects mangling)
- Different from `noexcept` (since C++17, not part of canonical type)

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

## Part 7: Grammar — Traditional EH vs Herbception Separation

### 7.1 Complete Separation (C++)

Herbception uses distinct keywords to avoid conflict with traditional EH:

| Feature | Traditional EH | Herbception (C++) |
|---------|----------------|-------------------|
| **Throw** | `throw expr;` | `throw throws expr;` |
| **Catch** | `catch(type e) { }` | `catch throws(type e) { }` |
| **Try block** | `try { } catch(type) { }` | Same try block, different catch |
| **Function spec** | `throw(T1, T2)` (deprecated) | `throws` or `fails{E}` |
| **Explicit handling** | N/A | `try(expr)` optional |

### 7.2 Function Specifiers — Works in Both C++ and C

**Two forms:**

```cpp
// Basic throws - uses std::error (implicit error type) — C++ only
T foo() throws;

// Typed fails - explicit error type E — works in BOTH C++ and C
T foo() fails{E};
```

**Why `{E}` instead of `(E)`?**
- `()` typically means conditional statement (e.g., `noexcept(cond)`)
- `{E}` clearly indicates a type parameter
- Consistent with C23 keyword approach (no `_` prefix macros)

### 7.3 C++ `throws` — Value-Based, Not Type-Based

**Key principle:** Herbception is **value-based**, not type-based. No exception types like traditional EH.

**Traditional EH (type-based):**
```cpp
throw std::runtime_error("error");  // type-based exception
throw MyCustomException(42);        // different type
```

**Herbception `throws` (value-based):**
```cpp
// throws only allows enums convertible to std::error
throw throws std::errc::file_not_found;
throw throws std::nt_errc::invalid_parameter;
throw throws std::win32_errc::access_denied;
throw throws std::wine_errc::wine_server_error;
```

**No `std::error` constructor:** You don't construct `std::error` directly. You throw enum values that are convertible to `std::error`.

**Standard error enums:**
```cpp
// Existing (always available)
enum class errc { ... };  // POSIX errno values (std::errc already exists)

// New (available when _WIN32 defined)
enum class nt_errc { ... };     // NTSTATUS values
enum class win32_errc { ... };  // Win32 error codes (GetLastError)
enum class com_errc { ... };    // COM HRESULT values
enum class wine_errc { ... };   // Host environment POSIX errno (for calling host libc)
```

**Note:** `std::errc` is an existing C++ standard enum for POSIX errno values. The new enums (`nt_errc`, `win32_errc`, `com_errc`, `wine_errc`) extend this pattern for Windows.

**`wine_errc` purpose:** Represents host environment POSIX errno in a general way, allowing direct calls to host libc APIs. The errno values are cross-platform and defined by Wine for Windows programs running under Wine.

**`std::error` is a magic type:**
```cpp
// std::error is not constructible directly
// std::error auto-converts from any *_errc enum
std::error e = std::errc::file_not_found;  // implicit conversion
std::error e = std::nt_errc::invalid_parameter;  // implicit conversion
```

**`std::error` implementation — concrete skeleton:**

```cpp
// io_scatter_t — basically iovec (for scatter/gather IO)
struct io_scatter_t {
  void const* base;
  std::size_t len;
};

// error_reporter_encoding — avoids unnecessary codecvt (e.g., on Windows)
enum class error_reporter_encoding {
  utf8,
  utf16,    // native endian
  utf32,    // native endian
  gb18030,
  utfebcdic
};

// error_reporter_io_cookie_function — IO callback for freestanding
using error_reporter_io_cookie_function = void (*)(error_reporter_encoding, void*, io_scatter_t const*, std::size_t) noexcept;

// error_domain_singleton — custom "vtable" (not C++ vtable)
struct error_domain_singleton {
  bool (*do_equivalent)(std::size_t, error_domain_singleton const*, std::size_t) noexcept;
  void (*do_name)(std::size_t, error_reporter_encoding, void*, error_reporter_io_cookie_function) noexcept;
  void (*do_message)(std::size_t, error_reporter_encoding, void*, error_reporter_io_cookie_function) noexcept;
  std::errc (*do_to_errc)(std::size_t) noexcept;
  void (*do_cleanup)(std::size_t) noexcept;  // called when std::error goes out of scope
};

// std::error — 2-register type (trivially relocatable, constexpr)
struct error {
  error_domain_singleton const* domain_opaque{};  // Register 1: domain singleton
  std::size_t code_opaque{};                       // Register 2: error code
  
  // Destructor — calls domain's do_cleanup
  constexpr ~error() noexcept {
    if (domain_opaque && domain_opaque->do_cleanup) {
      domain_opaque->do_cleanup(code_opaque);
    }
  }
  
  // Move constructor — nullify source to prevent double cleanup
  constexpr error(error&& other) noexcept 
    : domain_opaque(other.domain_opaque), code_opaque(other.code_opaque) {
    other.domain_opaque = nullptr;
    other.code_opaque = 0;
  }
  
  // Move assignment — cleanup current, then move and nullify source
  constexpr error& operator=(error&& other) noexcept {
    if (this != &other) {
      // Cleanup current value
      if (domain_opaque && domain_opaque->do_cleanup) {
        domain_opaque->do_cleanup(code_opaque);
      }
      // Move from other
      domain_opaque = other.domain_opaque;
      code_opaque = other.code_opaque;
      // Nullify source
      other.domain_opaque = nullptr;
      other.code_opaque = 0;
    }
    return *this;
  }
  
  // Copy NOT allowed
  error(const error&) = delete;
  error& operator=(const error&) = delete;
  
  // Cross-domain comparison
  template<typename T>
  constexpr bool do_equivalent(T ec) noexcept {
    return domain_opaque->do_equivalent(code_opaque, error_domain<T>::domain(), ec);
  }
  
  // Convert to POSIX errc for strerror etc.
  constexpr std::errc do_to_errc() noexcept {
    return domain_opaque->do_to_errc(code_opaque);
  }
};

// Template for each error domain
template<typename T>
requires (std::is_enum_v<T>)
struct error_domain {
  using errc_type = T;
  static error_domain_singleton const* domain() noexcept;
  static std::size_t code(errc_type e) noexcept;
};

// Comparison operators
template<typename T>
requires (std::is_enum_v<T>)
constexpr bool operator==(std::error e, T t) noexcept {
  return error_domain<T>::code(t) == e.code_opaque &&
         error_domain<T>::domain() == e.domain_opaque;
}
```

**Domain singleton example (win32) — `constinit` for compile-time init:**
```cpp
namespace std::error_domains {
constinit error_domain_singleton __win32_error_domain {
  .do_equivalent = [](std::size_t cd, error_domain_singleton const* other, std::size_t othercd) noexcept {
    // Cross-domain: compare after converting both to errc
    return __win32_error_domain.do_to_errc(cd) == other->do_to_errc(othercd);
  },
  .do_name = [](std::size_t, error_reporter_encoding enc, void* cookie, auto cookfun) noexcept {
    // IO-based model: write domain name via cookfun (no heap allocation)
    // Accommodates fast_io and freestanding IO libraries
  },
  .do_message = [](std::size_t code, error_reporter_encoding enc, void* cookie, auto cookfun) noexcept {
    // IO-based: write error message directly (FormatMessage on Windows)
    // No string-based model — no heap allocation needed
  },
  .do_to_errc = [](std::size_t cd) noexcept -> std::errc {
    // Map Win32 errors to POSIX errc
    switch(static_cast<std::win32_errc>(cd)) {
      case std::win32_errc::success:        return static_cast<std::errc>(0);
      case std::win32_errc::file_not_found: return std::errc::no_such_file_or_directory;
      case std::win32_errc::access_denied:  return std::errc::permission_denied;
      default:                              return std::errc::invalid_argument;
    }
  }
};

// ABI-stable external C function with [[__gnu__::__weak__]] — overridable!
extern "C" [[__gnu__::__weak__]]
error_domain_singleton const* __cxa_error_domain_win32() noexcept {
  return __builtin_addressof(__win32_error_domain);  // use __builtin_addressof
}
}
```

**IO-based model (not string-based):**
```cpp
// error_reporter uses IO callbacks, not string allocation
// Accommodates fast_io and freestanding environments

// No heap dependencies, NO STRING TYPES:
// - do_name: writes directly via IO callback
// - do_message: writes directly via IO callback
// - No std::string, no std::string_view, no const char*
// - No heap allocation, no dynamic memory

// Freestanding-friendly:
// - Works without <string> header
// - Works with custom IO libraries (fast_io, etc.)
// - constinit ensures compile-time initialization
// - No dependencies on STL string infrastructure
```

**`[[__gnu__::__weak__]]` — overridable for flexibility:**
```cpp
// Allows freestanding environments to implement their own error domains
// Handles DLL linking issues (multiple implementations possible)
// Custom error domains for embedded/special environments

// Freestanding can override:
extern "C" [[__gnu__::__weak__]]
error_domain_singleton const* __cxa_error_domain_win32() noexcept {
  return &my_custom_win32_error_domain;  // custom implementation
}
```

**Cross-language interoperability:**
```cpp
// error_domain_singleton uses extern "C" ABI with function pointers
// NOT C++ vtables — any language can implement!

// C can implement error domains:
struct error_domain_singleton my_c_error_domain = {
  .do_equivalent = my_c_equivalent_func,
  .do_name = my_c_name_func,
  .do_message = my_c_message_func,
  .do_to_errc = my_c_to_errc_func,
};

// std::error is just 2 registers (pointer + code)
// No C++ RTTI or class hierarchy needed
// Pure ABI with C function pointers

// Cross-language error propagation:
// C++: throw throws std::win32_errc::file_not_found;
// C:   return failure(errno_value);  // C's fails{int}
```

**Why extern "C" ABI matters:**
- `error_domain_singleton` is plain struct with function pointers
- No C++ features: no virtual functions, no RTTI, no inheritance
- `__cxa_error_domain_*` functions are C ABI stable
- Any language can implement their own domain
- Enables EH communication between C++ and C

**ABI implications:**
```cpp
// std::error returned in 2 registers (like struct)
// RAX: domain_opaque pointer (to singleton)
// RDX: code_opaque value

// No heap allocation (domain is singleton with constinit)
// Trivially relocatable = can be moved without calling destructor
// Enables efficient passing and returning despite having destructor

// Domain singletons initialized at compile-time (constinit)
// No dynamic initialization order issues
```

**Usage with `throw throws` syntax:**
```cpp
// Throw enum value (convertible to std::error)
throw throws std::win32_errc::file_not_found;

// Catch std::error (handles all domains)
catch throws(std::error e) {
  // Strict comparison (same domain)
  if (e == std::win32_errc::file_not_found) { ... }
  
  // Cross-domain comparison
  if (e.do_equivalent(std::errc::no_such_file_or_directory)) { ... }
  
  // Convert to POSIX errc for strerror
  fprintf(stderr, "%s\n", strerror(static_cast<int>(e.do_to_errc())));
}
```

### 7.4 C++ `fails{E}` — C-Style Value Return

**`fails{E}` uses C-style syntax in both C++ and C:**
```cpp
// C++ fails{E} uses failure(expr), NOT throw throws
int foo() fails{int} {
  if (error_condition) {
    return failure(error_value);  // C-style value return
  }
  return success_value;
}

// NOT: throw throws E(...)  // WRONG - fails doesn't use throw throws
```

**Why value-based matters:**
- No type hierarchy complexity
- No RTTI needed
- Simple comparison: `e == std::errc::file_not_found`
- Cross-domain comparison: `e.is_equivalent(other_domain_value)`

### 7.5 Catch Syntax — `catch throws` for `throws`, `catch fails` for `fails{E}`

**Catching `throws` (std::error):**
```cpp
try {
  try foo();  // foo() throws
} catch throws(std::error e) {
  // std::error handles all error domains
  
  // Strict comparison (same domain)
  if (e == std::errc::file_not_found) {
    // POSIX error
  }
  else if (e == std::win32_errc::access_denied) {
    // Win32 error
  }
  
  // Cross-domain comparison
  else if (e.is_equivalent(std::errc::permission_denied)) {
    // Matches equivalent error from any domain
  }
}
```

**Catching `fails{E}`:**
```cpp
// C++ and C use same syntax
try {
  try foo();  // foo() fails{int}
} catch fails(int e) {
  // Direct value comparison
  if (e == 42) { ... }
}
```

**Multiple catches:**
```cpp
try {
  try foo();  // foo() throws
  try bar();  // bar() fails{int}
} catch(std::exception& e) {
  // traditional EH
} catch throws(std::error e) {
  // herbception for throws
} catch fails(int e) {
  // herbception for fails{int}
}
```

### 7.6 Complete Examples

**`throws` function (C++ only):**
```cpp
int open_file(const char* path) throws {
  int fd = ::open(path, O_RDONLY);
  if (fd < 0) {
    throw throws std::errc(errno);  // errno → errc enum
  }
  return fd;
}

// Usage:
try {
  int fd = try open_file("test.txt");
} catch throws(std::error e) {
  if (e == std::errc::no_such_file_or_directory) {
    // file not found
  }
}
```

**`fails{E}` function (C++ and C):**
```cpp
// C++
int divide(int a, int b) fails{int} {
  if (b == 0) {
    return failure(EDIVIDE);  // C-style value return
  }
  return a / b;
}

// C
int divide(int a, int b) fails{int} {
  if (b == 0) {
    return failure(EDIVIDE);
  }
  return a / b;
}

// Both use same calling/handling:
int result = try divide(10, 2);  // auto-propagate
either(int, int) e = catch fails(divide(10, 0));  // get either
```

```cpp
// Option 1: catch fails(expr) at call point — returns either<T, E>
int foo() fails{int} {
  auto result = catch fails(bar());  // returns either<int, int>
  // OR: either(int, int) result = catch fails(bar());
  
  if (result.positive) {
    return result.left;  // success value
  } else {
    return failure(result.right);  // propagate error
  }
}

// Option 2: try(expr) at call point — auto-propagate on failure
int foo() fails{int} {
  int v = try(bar());  // if bar fails, auto-propagate error
  return v;            // only reached if bar succeeded
}

// Option 3: try before statement (no parens) — auto-propagate on failure
int foo() fails{int} {
  try bar();           // try before statement, no parentheses
  // ... continue if succeeded, auto-propagate if failed
  return 0;
}
```

**Key rule:** Calling `fails{E}` function without `try` or `catch fails` wrapper is a compile-time error in BOTH C++ and C.

### 7.6 Catch Syntax — `catch throws` vs `catch fails`

| Function Specifier | Throw Syntax | Catch Syntax |
|--------------------|--------------|--------------|
| `throws` (C++ only) | `throw throws expr` | `catch throws(std::error e)` |
| `fails{E}` (C++ and C) | `failure(expr)` | `catch fails(E e)` |

**Example:**
```cpp
// For throws (implicit std::error) — C++ only
try { } catch throws(std::error e) { }

// For fails{E} (explicit error type) — works in both C++ and C
try { } catch fails(int e) { }
```

### 7.7 Complete C++ Example with `fails{E}`

```cpp
// Function with typed error
int some_function(int x) fails{float} {
  return (x != 0) ? 5 : failure(2.0);
}

// Option 1: catch fails returns either
int foo() fails{float} {
  auto result = catch fails(some_function(x));
  if (result.positive) {
    return result.left + 1;
  } else {
    return failure(result.right);
  }
}

// Option 2: try(expr) auto-propagates
int bar() fails{float} {
  int v = try(some_function(x));
  return v + 1;  // only if succeeded
}

// Option 3: try statement auto-propagates
int baz() fails{float} {
  try some_function(x);
  return 0;  // only if succeeded
}

// With catch block
int qux() {
  try {
    try some_function(x);
  } catch fails(float e) {
    printf("error: %f\n", e);
    return -1;
  }
  return 0;
}
```

### 7.8 Comparison: C++ vs C `fails{E}` Handling

| Aspect | C++ | C |
|--------|-----|---|
| **Catch returns either** | `catch fails(expr)` → `either<T,E>` | `catch fails(expr)` → `either<T,E>` |
| **Auto-propagate at call** | `try(expr)` | `try(expr)` |
| **Auto-propagate at statement** | `try expr;` (no parens) | `try(expr)` (parens required) |
| **Catch block syntax** | `catch fails(E e)` | `catch fails(expr)` (expression, not block) |
| **Calling without wrapper** | **Compile error** | **Compile error** |

**Both C++ and C enforce explicit handling for `fails{E}` functions.**

### 7.5 C Language Syntax (C23-style Keywords)

Following C23's approach (keywords without `_` prefix, like `bool` instead of `_Bool`):

| Feature | C Syntax | Notes |
|---------|----------|-------|
| **Function spec** | `T foo() fails{E}` | `{E}` not `(E)` to avoid conditional confusion |
| **Return failure** | `failure(expr)` | Return via failure channel |
| **Error propagation** | `try(expr)` | Auto-propagate failure (like `_Try` in proposal) |
| **Error catch** | `catch fails(expr)` | Convert to `either{T,E}` (consistent with C++) |
| **Sum type** | `either{T, E}` | `.left` = T, `.right` = E, `.positive` = success |

### 7.6 C Language Example

```c
// Function with typed error
int some_function(int x) fails{float} {
  return (x != 0) ? 5 : failure(2.0);  // Return via failure channel
}

const char *some_other_function(int x) fails{float} {
  // try() auto-propagates failure: if some_function fails, return its error
  int v = try(some_function(x));
  
  return (v == 5) ? "Yes" : "No";
}

int main(int argc, char *argv[]) {
  // catch fails() converts result to either type
  either(const char *, float) v = catch fails(some_other_function(atoi(argv[1])));
  
  if(v.positive) {
    printf("success: %s\n", v.left);
  } else {
    printf("failure: %f\n", v.right);
  }
  return 0;
}
```

### 7.7 C Requires Explicit Error Handling — No Invisible Propagation

**Critical difference from C++:**

| Aspect | C++ | C |
|--------|-----|---|
| **Error catching** | Implicit (compiler checks discriminant) | Explicit: MUST use `try(expr)` or `catch fails(expr)` |
| **Error propagation** | Automatic (discriminant-based branch) | Manual: `try(expr)` auto-propagates, otherwise compile error |
| **Calling fails function** | Allowed anywhere | Compile error if not wrapped in `try()` or `catch fails()` |

**C compile-time rule:**
Calling a `fails{E}` function without `try(expr)` or `catch fails(expr)` wrapper is a **compile-time error**. This forces explicit error handling.

```c
// WRONG in C - compile error!
int foo() fails{int} {
  int x = some_fails_func();  // ERROR: must use try() or catch fails()
  return x;
}

// CORRECT in C - explicit error handling
int foo() fails{int} {
  int x = try(some_fails_func());  // OK: try() catches and auto-propagates
  return x;
}

// ALSO CORRECT - convert to either
int bar() {
  either(int, int) result = catch fails(some_fails_func());
  if (result.positive) {
    return result.left;
  } else {
    handle_error(result.right);
    return -1;
  }
}
```

**C++ allows implicit handling:**
```cpp
// C++ - OK, compiler implicitly handles discriminant
int foo() throws {
  auto result = some_throws_func();  // compiler checks discriminant
  if (result.failed) {
    // handle or propagate
  }
  return result.value;
}
```

### 7.8 Syntax Comparison: C vs C++

| Feature | C++ | C |
|---------|-----|---|
| **Function specifier** | `throws` or `fails{E}` | `fails{E}` only |
| **Implicit error type** | `std::error` (for `throws`) | N/A (must specify E) |
| **Return error** | `throw throws expr` | `failure(expr)` |
| **Error catching** | **Required for `fails{E}`**: `try(expr)` or `catch fails(expr)` | **Required**: `try(expr)` or `catch fails(expr)` |
| **Auto-propagate** | `try expr` or `try(expr)` | `try(expr)` |
| **Convert to sum** | `catch fails(expr)` → `either{T,E}` | `catch fails(expr)` → `either{T,E}` |
| **Catch block** | `catch fails(E e) { }` | N/A (use `catch fails(expr)` expression) |
| **Sum type** | `either{T, E}` | `either{T, E}` |
| **Compile enforcement** | **Error for `fails{E}` without wrapper** | **Error if fails function called without wrapper** |

### 7.9 Parser Implementation

**File:** `clang/lib/Parse/ParseDeclCXX.cpp`

**Function specifier parsing:**
```cpp
// After parsing function return type and name
if (Tok.is(tok::kw_throws)) {
  ConsumeToken();
  // EST_BasicThrows - implicit std::error
} else if (Tok.is(tok::kw_fails)) {
  ConsumeToken();
  if (Tok.is(tok::l_brace)) {
    ConsumeBrace();
    QualType ErrorType = ParseTypeName();
    ExpectAndConsume(tok::r_brace);
    // EST_FailsTyped with explicit error type
  }
}
```

**File:** `clang/lib/Parse/ParseExprCXX.cpp`

**Throw parsing (C++):**
```cpp
if (Tok.is(tok::kw_throw)) {
  ConsumeToken();
  if (Tok.is(tok::kw_throws)) {
    // Herbception: throw throws expr
    ConsumeToken();
    ExprResult E = ParseExpression();
    return Actions.ActOnThrowThrowsExpr(ThrowLoc, ThrowsLoc, E);
  } else if (Tok.isNot(tok::semi)) {
    // Traditional: throw expr
    ExprResult E = ParseExpression();
    return Actions.ActOnThrowExpr(ThrowLoc, E);
  }
}
```

**File:** `clang/lib/Parse/ParseExpr.c`

**Failure parsing (C):**
```cpp
if (Tok.is(tok::kw_failure)) {
  ConsumeToken();
  ExpectAndConsume(tok::l_paren);
  ExprResult E = ParseExpression();
  ExpectAndConsume(tok::r_paren);
  return Actions.ActOnFailureExpr(FailureLoc, E);
}
```

**Try parsing (C):**
```cpp
if (Tok.is(tok::kw_try)) {
  ConsumeToken();
  ExpectAndConsume(tok::l_paren);
  ExprResult E = ParseExpression();
  ExpectAndConsume(tok::r_paren);
  return Actions.ActOnTryExpr(TryLoc, E);
}
```

**Catch parsing (C):**
```cpp
if (Tok.is(tok::kw_catch)) {
  ConsumeToken();
  if (Tok.is(tok::kw_fails)) {
    ConsumeToken();
    ExpectAndConsume(tok::l_paren);
    ExprResult E = ParseExpression();
    ExpectAndConsume(tok::r_paren);
    return Actions.ActOnCatchFailsExpr(CatchLoc, E);
  }
}
```

**File:** `clang/lib/Parse/ParseStmt.cpp`

**Catch parsing (C++):**
```cpp
while (Tok.is(tok::kw_catch)) {
  ConsumeToken();
  
  if (Tok.is(tok::kw_throws)) {
    // Herbception: catch throws(type e)
    ConsumeToken();
    ParseCatchesThrowsClause();
  } else {
    // Traditional: catch(type e)
    ParseTraditionalCatchClause();
  }
}
```

### 7.9 AST Nodes

**C++ AST nodes:**
```cpp
// CXXThrowThrowsExpr - herbception throw (throw throws expr)
class CXXThrowThrowsExpr : public Expr {
  Expr *ErrorValue;
  SourceLocation ThrowsLoc;
};

// CXXCatchThrowsStmt - herbception catch (catch throws(e) { })
class CXXCatchThrowsStmt : public Stmt {
  VarDecl *ErrorDecl;
  Stmt *HandlerBlock;
};
```

**C AST nodes:**
```cpp
// FailureExpr - failure(expr)
class FailureExpr : public Expr {
  Expr *ErrorValue;
};

// TryExpr - try(expr)
class TryExpr : public Expr {
  Expr *SubExpr;  // auto-propagates failure
};

// CatchFailsExpr - catch fails(expr)
class CatchFailsExpr : public Expr {
  Expr *SubExpr;  // converts to either<T, E>
};

// EitherType - either{T, E}
class EitherType : public Type {
  QualType LeftType;   // success type T
  QualType RightType;  // failure type E
};
```

**File:** `clang/include/clang/AST/StmtCXX.h` (C++)
**File:** `clang/include/clang/AST/Stmt.h` (C)

### 7.10 Semantic Analysis

**File:** `clang/lib/Sema/SemaExprCXX.cpp`

```cpp
Sema::ActOnThrowThrowsExpr(SourceLocation ThrowLoc, SourceLocation ThrowsLoc,
                           Expr *ErrorValue) {
  // Check that function has throws/fails specifier
  FunctionDecl *FD = getCurrentFunctionDecl();
  if (!FD->hasThrowsOrFailsSpecifier()) {
    Diag(ThrowLoc, diag::err_throw_throws_without_spec);
    return ExprError();
  }
  
  // Check error type matches function's declared error type
  QualType ErrorType = ErrorValue->getType();
  QualType ExpectedErrorType = FD->getErrorType();  // std::error or E from fails{E}
  
  return new (Context) CXXThrowThrowsExpr(ErrorValue, ThrowsLoc);
}
```

**File:** `clang/lib/Sema/SemaExpr.c`

```cpp
Sema::ActOnTryExpr(SourceLocation TryLoc, Expr *SubExpr) {
  // try(expr) auto-propagates failure:
  // If expr fails: return failure(expr.right)
  // If expr succeeds: use expr.left
  
  // Check that current function has matching fails specifier
  FunctionDecl *FD = getCurrentFunctionDecl();
  if (!FD->hasFailsSpecifier()) {
    Diag(TryLoc, diag::err_try_without_fails_spec);
    return ExprError();
  }
  
  return new (Context) TryExpr(SubExpr);
}

// C compile-time enforcement: calling fails function without wrapper is error
Sema::CheckCallExpr(CallExpr *CE) {
  FunctionDecl *Callee = CE->getCalleeDecl();
  
  // In C mode: check if callee has fails specifier
  if (getLangOpts().C99 && Callee->hasFailsSpecifier()) {
    // Check if this call is inside try() or catch fails() wrapper
    Expr *Parent = getParentExpr(CE);
    if (!isTryOrCatchFailsWrapper(Parent)) {
      Diag(CE->getBeginLoc(), diag::err_fails_call_without_wrapper);
      Diag(Callee->getLocation(), diag::note_fails_function_declared_here);
      return ExprError();
    }
  }
}
```

**Error message example:**
```
error: calling function with 'fails{E}' specifier requires 'try()' or 'catch fails()' wrapper
  int x = some_fails_func();
          ^~~~~~~~~~~~~~~
note: function declared with 'fails{int}' here
  int some_fails_func() fails{int};
      ^

// Correct usage:
  int x = try(some_fails_func());       // auto-propagate
  either(int, int) e = catch fails(some_fails_func());  // convert to either
```

### 7.11 Code Generation

**Herbception throw (C++):**
```cpp
void CodeGenFunction::EmitCXXThrowThrowsExpr(const CXXThrowThrowsExpr *E) {
  llvm::Value *ErrorVal = EmitScalarExpr(E->getErrorValue());
  
  // Emit destructors for current scope (normal cleanup)
  EmitCleanupForScopeExit();
  
  // Return with discriminant set
  llvm::Value *Failed = Builder.getInt1(1);
  llvm::Value *Result = Builder.CreateInsertValue(
      llvm::UndefValue::get(ResultType), ErrorVal, 0);
  Result = Builder.CreateInsertValue(Result, Failed, 1);
  Builder.CreateRet(Result);
}
```

**Failure expression (C):**
```cpp
void CodeGenFunction::EmitFailureExpr(const FailureExpr *E) {
  llvm::Value *ErrorVal = EmitScalarExpr(E->getErrorValue());
  
  // Return via failure channel
  llvm::Value *Failed = Builder.getInt1(1);
  llvm::Value *Result = Builder.CreateInsertValue(
      llvm::UndefValue::get(ResultType), ErrorVal, 0);
  Result = Builder.CreateInsertValue(Result, Failed, 1);
  Builder.CreateRet(Result);
}
```

**Try expression (C) — auto-propagate:**
```cpp
void CodeGenFunction::EmitTryExpr(const TryExpr *E) {
  // Call the sub-expression
  llvm::Value *Result = EmitScalarExpr(E->getSubExpr());
  
  // Check discriminant
  llvm::Value *Failed = Builder.CreateExtractValue(Result, 1);
  
  // If failed: propagate error (return failure)
  llvm::BasicBlock *PropagateBB = createBasicBlock("try.propagate");
  llvm::BasicBlock *SuccessBB = createBasicBlock("try.success");
  
  Builder.CreateCondBr(Builder.CreateICmpNE(Failed, Builder.getInt1(0)),
                       PropagateBB, SuccessBB);
  
  // Propagate block: return the error
  EmitBlock(PropagateBB);
  Builder.CreateRet(Result);  // propagate error
  
  // Success block: continue with value
  EmitBlock(SuccessBB);
  llvm::Value *Value = Builder.CreateExtractValue(Result, 0);
  // Value is now available for subsequent use
}
```

**Catch fails expression (C) — convert to either:**
```cpp
void CodeGenFunction::EmitCatchFailsExpr(const CatchFailsExpr *E) {
  // Call the sub-expression
  llvm::Value *Result = EmitScalarExpr(E->getSubExpr());
  
  // Create either<T, E> result
  llvm::Type *EitherTy = getEitherType(E->getType());
  llvm::Value *Either = llvm::UndefValue::get(EitherTy);
  
  // .positive = !failed
  llvm::Value *Failed = Builder.CreateExtractValue(Result, 1);
  llvm::Value *Positive = Builder.CreateICmpEQ(Failed, Builder.getInt1(0));
  Either = Builder.CreateInsertValue(Either, Positive, 0);  // .positive
  
  // Extract value or error based on discriminant
  llvm::Value *Payload = Builder.CreateExtractValue(Result, 0);
  
  // Create union storage for .left/.right
  // If positive: store in .left
  // If negative: store in .right
  Either = Builder.CreateInsertValue(Either, Payload, 1);  // union
  
  // Note: caller accesses .left or .right based on .positive
}
```

---

## Part 8: Function Pointer Compatibility

### 8.1 Strict Type Separation

Function pointers with `throws/fails` are **distinct types** — no implicit conversions allowed either way:

```cpp
// Different types — no implicit conversion
void (*p1)(int);                    // normal function pointer
void (*p2)(int) throws;             // throws function pointer — different type!
void (*p3)(int) fails{E};           // fails function pointer — different type!

// These are compile errors:
p1 = p2;  // ERROR: cannot assign throws pointer to non-throws pointer
p2 = p1;  // ERROR: cannot assign non-throws pointer to throws pointer
p1 = p3;  // ERROR: cannot assign fails pointer to non-fails pointer
p3 = p1;  // ERROR: cannot assign non-fails pointer to fails pointer
```

### 8.2 Comparison with `noexcept`

This mirrors the existing `noexcept` situation:

```cpp
// noexcept function pointers (existing C++17 behavior)
void (*p1)(int);           // can throw
void (*p2)(int) noexcept;  // cannot throw

// C++17 allows: noexcept → non-noexcept (safe conversion)
p1 = p2;  // OK: noexcept function can be called as non-noexcept

// C++17 disallows: non-noexcept → noexcept (unsafe)
p2 = p1;  // ERROR: throwing function cannot be called as noexcept
```

**Why `throws/fails` is stricter:**
- `noexcept`: calling convention is same, just adds optimization hint
- `throws/fails`: calling convention **changes** (struct return vs discriminant)
- Implicit conversion would cause ABI mismatch at runtime

### 8.3 Use Cases for `throws/fails` Function Pointers

**Calling C APIs with error handling:**
```cpp
// POSIX open() returns -1 on error, sets errno
// Can be wrapped as fails function:
int open_wrapped(const char* path, int flags) fails{int} {
  int fd = open(path, flags);
  if (fd < 0) return failure(errno);
  return fd;
}

// Store function pointer for later use
int (*open_func)(const char*, int) fails{int} = open_wrapped;
```

**Callback patterns:**
```cpp
// Callback that might fail
using ErrorCallback = void(*)(int error_code) fails{int};

// Register callback
void register_callback(ErrorCallback cb) {
  // cb must be fails{int} function pointer
}
```

### 8.4 Canonical Type Rules

**Similar to `noexcept` in C++17:**

| Specifier | Part of canonical type? | Conversion allowed? |
|-----------|------------------------|---------------------|
| `noexcept` | No (since C++17) | noexcept → non-noexcept OK |
| `throws` | **Yes** | No conversions |
| `fails{E}` | **Yes** | No conversions |

**Implementation:**
```cpp
// clang/lib/AST/ASTContext.cpp
QualType ASTContext::getCanonicalType(QualType T) {
  // throws/fails affects canonical type (unlike noexcept since C++17)
  if (const FunctionProtoType *FPT = T->getAs<FunctionProtoType>()) {
    if (FPT->hasThrowsOrFailsSpecifier()) {
      // throws/fails is part of canonical type
      return getFunctionTypeWithThrowsSpec(...);
    }
  }
}
```

### 8.5 Template Deduction

```cpp
// Template deduction respects throws/fails
template<typename F>
void call_func(F func);

void normal_func(int);
void throws_func(int) throws;
void fails_func(int) fails{int};

// Each deduces different F:
call_func(normal_func);  // F = void(*)(int)
call_func(throws_func);  // F = void(*)(int) throws
call_func(fails_func);   // F = void(*)(int) fails{int}

// Cannot mix:
template<typename F>
void wrapper(F func) throws {
  func(42);  // ERROR if F is non-throws/non-fails
}
```

### 8.6 ABI Implications

| Function Type | Return ABI | Caller responsibility |
|---------------|------------|----------------------|
| Normal | T in register(s) | No error checking |
| `throws` | `{T, i1}` or discriminant | Must check discriminant |
| `fails{E}` | `{T, i1}` or discriminant | Must check discriminant |

**Mixing would cause:**
- Caller expects normal return, callee returns struct → corruption
- Caller expects discriminant, callee returns normal → undefined behavior

### 8.7 Sema Implementation

**File:** `clang/lib/Sema/SemaExpr.cpp`

```cpp
Sema::CheckFunctionPointerAssignment(QualType ToType, QualType FromType) {
  const FunctionProtoType *ToFPT = ToType->getAs<FunctionProtoType>();
  const FunctionProtoType *FromFPT = FromType->getAs<FunctionProtoType>();
  
  // Check throws/fails specifier match
  bool ToHasThrows = ToFPT && ToFPT->hasThrowsSpecifier();
  bool FromHasThrows = FromFPT && FromFPT->hasThrowsSpecifier();
  bool ToHasFails = ToFPT && ToFPT->hasFailsSpecifier();
  bool FromHasFails = FromFPT && FromFPT->hasFailsSpecifier();
  
  // No implicit conversions allowed
  if (ToHasThrows != FromHasThrows || ToHasFails != FromHasFails) {
    Diag(Loc, diag::err_func_ptr_throws_mismatch);
    return false;
  }
  
  // If both have fails, error types must match
  if (ToHasFails && FromHasFails) {
    QualType ToErrorType = ToFPT->getFailsErrorType();
    QualType FromErrorType = FromFPT->getFailsErrorType();
    if (!Context.hasSameType(ToErrorType, FromErrorType)) {
      Diag(Loc, diag::err_func_ptr_fails_type_mismatch);
      return false;
    }
  }
  
  return true;
}
```

---

## Part 9: Implementation Phases

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
| **Parsing** | `clang/lib/Parse/ParseDeclCXX.cpp` | 3923 (tryParseExceptionSpecification) |
| | `clang/lib/Parse/ParseExprCXX.cpp` | — |
| | `clang/include/clang/Basic/TokenKinds.def` | — |
| **AST/Types** | `clang/include/clang/AST/TypeBase.h` | 2013 (ExceptionSpecType bit), 5678 (getExceptionSpecType) |
| | `clang/lib/AST/Type.cpp` | — |
| **Exception Spec** | `clang/include/clang/Basic/ExceptionSpecificationType.h` | 21-32 (EST_ enum), 35-55 (helpers) |
| **Sema** | `clang/lib/Sema/SemaExceptionSpec.cpp` | — |
| | `clang/lib/Sema/SemaExprCXX.cpp` | — |
| **CodeGen** | `clang/lib/CodeGen/CGCall.cpp` | 2018 (isNothrow → nounwind) |
| | `clang/lib/CodeGen/CGException.cpp` | 485, 508, 541 (pushTerminate) |
| | `clang/lib/CodeGen/ItaniumCXXABI.cpp` | — |
| | `clang/include/clang/CodeGen/CGFunctionInfo.h` | — |
| | `clang/lib/CodeGen/Targets/X86.cpp` | — |
| **Mangling** | `clang/lib/AST/ItaniumMangle.cpp` | — |
| **Printing** | `clang/lib/AST/TypePrinter.cpp` | — |

### LLVM Files

| Category | File | Key Lines |
|----------|------|-----------|
| **Attributes** | `llvm/include/llvm/IR/Attributes.td` | — (swifterror ref) |
| | `llvm/include/llvm/IR/Attributes.h` | — |
| **Calling Conv** | `llvm/include/llvm/IR/CallingConv.h` | 21 (namespace), 304 (end) |
| | `llvm/include/llvm/CodeGen/CallingConvLower.h` | — |
| | `llvm/include/llvm/CodeGen/TargetCallingConv.h` | — |
| | `llvm/include/llvm/Target/TargetCallingConv.td` | — |
| **TargetLowering** | `llvm/include/llvm/CodeGen/TargetLowering.h` | — |
| **SelectionDAG** | `llvm/lib/CodeGen/SelectionDAG/SelectionDAGBuilder.cpp` | — |
| | `llvm/lib/CodeGen/FunctionLoweringInfo.cpp` | — |
| **X86** | `llvm/lib/Target/X86/X86CallingConv.td` | — |
| | `llvm/lib/Target/X86/X86ISelLowering.cpp` | 25648, 28601 (COND_B usage) |
| | `llvm/lib/Target/X86/X86ISelLoweringCall.cpp` | — |
| | `llvm/lib/Target/X86/X86RegisterInfo.td` | — |
| | `llvm/lib/Target/X86/MCTargetDesc/X86BaseInfo.h` | — (CondCode enum) |
| | `llvm/lib/Target/X86/X86InstrMisc.td` | — (CLC/STC) |
| | `llvm/lib/Target/X86/X86InstrCompiler.td` | — (SETB_C) |
| **AArch64** | `llvm/lib/Target/AArch64/Utils/AArch64BaseInfo.h` | 288 (CondCode enum) |
| | `llvm/lib/Target/AArch64/AArch64RegisterInfo.td` | — (NZCV) |
| | `llvm/lib/Target/AArch64/AArch64ISelLowering.cpp` | — |
| | `llvm/lib/Target/AArch64/AArch64InstrInfo.td` | — |
| **RISC-V** | `llvm/lib/Target/RISCV/RISCVCallingConv.cpp` | 167 (ArgIGPRs), 402-410 (return limit) |
| | `llvm/lib/Target/RISCV/RISCVRegisterInfo.td` | — |
| | `llvm/lib/Target/RISCV/RISCVISelLowering.cpp` | — |
| **LoongArch** | `llvm/lib/Target/LoongArch/LoongArchISelLowering.cpp` | — |
| | `llvm/lib/Target/LoongArch/LoongArchRegisterInfo.td` | — |
| **ISD Opcodes** | `llvm/include/llvm/CodeGen/ISDOpcodes.h` | — |
| **Intrinsics** | `llvm/include/llvm/IR/Intrinsics.td` | — |
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