# NOTE: Assertions have been autogenerated by utils/update_mca_test_checks.py
# RUN: llvm-mca -mtriple=riscv64 -mcpu=sifive-p870 -iterations=1 -instruction-tables=full < %s | FileCheck %s

mul s6, s6, s7

mulw s4, s4, a2

cpop t1, t1

cpopw t2, t2

# CHECK:      Resources:
# CHECK-NEXT: [0]   - SiFiveP800Branch:2 SiFiveP800IEXQ4, SiFiveP800IEXQ5
# CHECK-NEXT: [1]   - SiFiveP800Div:1
# CHECK-NEXT: [2]   - SiFiveP800FEXQ0:1
# CHECK-NEXT: [3]   - SiFiveP800FEXQ1:1
# CHECK-NEXT: [4]   - SiFiveP800FloatArith:2 SiFiveP800FEXQ0, SiFiveP800FEXQ1
# CHECK-NEXT: [5]   - SiFiveP800FloatDiv:1
# CHECK-NEXT: [6]   - SiFiveP800IEXQ0:1
# CHECK-NEXT: [7]   - SiFiveP800IEXQ1:1
# CHECK-NEXT: [8]   - SiFiveP800IEXQ2:1
# CHECK-NEXT: [9]   - SiFiveP800IEXQ3:1
# CHECK-NEXT: [10]  - SiFiveP800IEXQ4:1
# CHECK-NEXT: [11]  - SiFiveP800IEXQ5:1
# CHECK-NEXT: [12]  - SiFiveP800IntArith:4 SiFiveP800IEXQ0, SiFiveP800IEXQ1, SiFiveP800IEXQ2, SiFiveP800IEXQ3
# CHECK-NEXT: [13]  - SiFiveP800LD:1
# CHECK-NEXT: [14]  - SiFiveP800LDST:2
# CHECK-NEXT: [15]  - SiFiveP800Load:3 SiFiveP800LDST, SiFiveP800LDST, SiFiveP800LD
# CHECK-NEXT: [16]  - SiFiveP800Mul:2 SiFiveP800IEXQ1, SiFiveP800IEXQ3
# CHECK-NEXT: [17]  - SiFiveP800VDiv:1
# CHECK-NEXT: [18]  - SiFiveP800VEXQ0:1
# CHECK-NEXT: [19]  - SiFiveP800VEXQ1:1
# CHECK-NEXT: [20]  - SiFiveP800VFloatDiv:1
# CHECK-NEXT: [21]  - SiFiveP800VLD:1
# CHECK-NEXT: [22]  - SiFiveP800VST:1
# CHECK-NEXT: [23]  - SiFiveP800VectorArith:2 SiFiveP800VEXQ0, SiFiveP800VEXQ1

# CHECK:      Instruction Info:
# CHECK-NEXT: [1]: #uOps
# CHECK-NEXT: [2]: Latency
# CHECK-NEXT: [3]: RThroughput
# CHECK-NEXT: [4]: MayLoad
# CHECK-NEXT: [5]: MayStore
# CHECK-NEXT: [6]: HasSideEffects (U)
# CHECK-NEXT: [7]: Bypass Latency
# CHECK-NEXT: [8]: Resources (<Name> | <Name>[<ReleaseAtCycle>] | <Name>[<AcquireAtCycle>,<ReleaseAtCycle])
# CHECK-NEXT: [9]: LLVM Opcode Name

# CHECK:      [1]    [2]    [3]    [4]    [5]    [6]    [7]    [8]                                        [9]                        Instructions:
# CHECK-NEXT:  1      2     0.50                         2     SiFiveP800IntArith,SiFiveP800Mul           MUL                        mul	s6, s6, s7
# CHECK-NEXT:  1      2     0.50                         2     SiFiveP800IntArith,SiFiveP800Mul           MULW                       mulw	s4, s4, a2
# CHECK-NEXT:  1      2     0.50                         2     SiFiveP800IntArith,SiFiveP800Mul           CPOP                       cpop	t1, t1
# CHECK-NEXT:  1      2     0.50                         2     SiFiveP800IntArith,SiFiveP800Mul           CPOPW                      cpopw	t2, t2

# CHECK:      Resources:
# CHECK-NEXT: [0]   - SiFiveP800Div
# CHECK-NEXT: [1]   - SiFiveP800FEXQ0
# CHECK-NEXT: [2]   - SiFiveP800FEXQ1
# CHECK-NEXT: [3]   - SiFiveP800FloatDiv
# CHECK-NEXT: [4]   - SiFiveP800IEXQ0
# CHECK-NEXT: [5]   - SiFiveP800IEXQ1
# CHECK-NEXT: [6]   - SiFiveP800IEXQ2
# CHECK-NEXT: [7]   - SiFiveP800IEXQ3
# CHECK-NEXT: [8]   - SiFiveP800IEXQ4
# CHECK-NEXT: [9]   - SiFiveP800IEXQ5
# CHECK-NEXT: [10]  - SiFiveP800LD
# CHECK-NEXT: [11.0] - SiFiveP800LDST
# CHECK-NEXT: [11.1] - SiFiveP800LDST
# CHECK-NEXT: [12]  - SiFiveP800VDiv
# CHECK-NEXT: [13]  - SiFiveP800VEXQ0
# CHECK-NEXT: [14]  - SiFiveP800VEXQ1
# CHECK-NEXT: [15]  - SiFiveP800VFloatDiv
# CHECK-NEXT: [16]  - SiFiveP800VLD
# CHECK-NEXT: [17]  - SiFiveP800VST

# CHECK:      Resource pressure per iteration:
# CHECK-NEXT: [0]    [1]    [2]    [3]    [4]    [5]    [6]    [7]    [8]    [9]    [10]   [11.0] [11.1] [12]   [13]   [14]   [15]   [16]   [17]
# CHECK-NEXT:  -      -      -      -      -     2.00    -     2.00    -      -      -      -      -      -      -      -      -      -      -

# CHECK:      Resource pressure by instruction:
# CHECK-NEXT: [0]    [1]    [2]    [3]    [4]    [5]    [6]    [7]    [8]    [9]    [10]   [11.0] [11.1] [12]   [13]   [14]   [15]   [16]   [17]   Instructions:
# CHECK-NEXT:  -      -      -      -      -     0.50    -     0.50    -      -      -      -      -      -      -      -      -      -      -     mul	s6, s6, s7
# CHECK-NEXT:  -      -      -      -      -     0.50    -     0.50    -      -      -      -      -      -      -      -      -      -      -     mulw	s4, s4, a2
# CHECK-NEXT:  -      -      -      -      -     0.50    -     0.50    -      -      -      -      -      -      -      -      -      -      -     cpop	t1, t1
# CHECK-NEXT:  -      -      -      -      -     0.50    -     0.50    -      -      -      -      -      -      -      -      -      -      -     cpopw	t2, t2
