// NOTE: Assertions have been autogenerated by utils/update_cc_test_checks.py
// RUN: %clang_cc1 -triple riscv64-none-linux-gnu -target-feature +f -target-feature +d -target-feature +zve64d -mvscale-min=1 -mvscale-max=1 -O1 -emit-llvm -o - %s | FileCheck %s --check-prefix=CHECK-64
// RUN: %clang_cc1 -triple riscv64-none-linux-gnu -target-feature +f -target-feature +d -target-feature +zve64d -mvscale-min=2 -mvscale-max=2 -O1 -emit-llvm -o - %s | FileCheck %s --check-prefix=CHECK-128

// REQUIRES: riscv-registered-target

#include <riscv_vector.h>

typedef vbool32_t fixed_bool32_t __attribute__((riscv_rvv_vector_bits(__riscv_v_fixed_vlen/32)));
typedef vbool64_t fixed_bool64_t __attribute__((riscv_rvv_vector_bits(__riscv_v_fixed_vlen/64)));

//===----------------------------------------------------------------------===//
// fixed, fixed
//===----------------------------------------------------------------------===//

// CHECK-64-LABEL: @call_bool32_ff(
// CHECK-64-NEXT:  entry:
// CHECK-64-NEXT:    [[TMP2:%.*]] = tail call <vscale x 2 x i1> @llvm.riscv.vmand.nxv2i1.i64(<vscale x 2 x i1> [[TMP0:%.*]], <vscale x 2 x i1> [[TMP1:%.*]], i64 2)
// CHECK-64-NEXT:    ret <vscale x 2 x i1> [[TMP2]]
//
// CHECK-128-LABEL: @call_bool32_ff(
// CHECK-128-NEXT:  entry:
// CHECK-128-NEXT:    [[TMP2:%.*]] = tail call <vscale x 2 x i1> @llvm.riscv.vmand.nxv2i1.i64(<vscale x 2 x i1> [[TMP0:%.*]], <vscale x 2 x i1> [[TMP1:%.*]], i64 4)
// CHECK-128-NEXT:    ret <vscale x 2 x i1> [[TMP2]]
//
fixed_bool32_t call_bool32_ff(fixed_bool32_t op1, fixed_bool32_t op2) {
  return __riscv_vmand(op1, op2, __riscv_v_fixed_vlen / 32);
}

// CHECK-64-LABEL: @call_bool64_ff(
// CHECK-64-NEXT:  entry:
// CHECK-64-NEXT:    [[TMP2:%.*]] = tail call <vscale x 1 x i1> @llvm.riscv.vmand.nxv1i1.i64(<vscale x 1 x i1> [[TMP0:%.*]], <vscale x 1 x i1> [[TMP1:%.*]], i64 1)
// CHECK-64-NEXT:    ret <vscale x 1 x i1> [[TMP2]]
//
// CHECK-128-LABEL: @call_bool64_ff(
// CHECK-128-NEXT:  entry:
// CHECK-128-NEXT:    [[TMP2:%.*]] = tail call <vscale x 1 x i1> @llvm.riscv.vmand.nxv1i1.i64(<vscale x 1 x i1> [[TMP0:%.*]], <vscale x 1 x i1> [[TMP1:%.*]], i64 2)
// CHECK-128-NEXT:    ret <vscale x 1 x i1> [[TMP2]]
//
fixed_bool64_t call_bool64_ff(fixed_bool64_t op1, fixed_bool64_t op2) {
  return __riscv_vmand(op1, op2, __riscv_v_fixed_vlen / 64);
}

//===----------------------------------------------------------------------===//
// fixed, scalable
//===----------------------------------------------------------------------===//

// CHECK-64-LABEL: @call_bool32_fs(
// CHECK-64-NEXT:  entry:
// CHECK-64-NEXT:    [[TMP1:%.*]] = tail call <vscale x 2 x i1> @llvm.riscv.vmand.nxv2i1.i64(<vscale x 2 x i1> [[TMP0:%.*]], <vscale x 2 x i1> [[OP2:%.*]], i64 2)
// CHECK-64-NEXT:    ret <vscale x 2 x i1> [[TMP1]]
//
// CHECK-128-LABEL: @call_bool32_fs(
// CHECK-128-NEXT:  entry:
// CHECK-128-NEXT:    [[TMP1:%.*]] = tail call <vscale x 2 x i1> @llvm.riscv.vmand.nxv2i1.i64(<vscale x 2 x i1> [[TMP0:%.*]], <vscale x 2 x i1> [[OP2:%.*]], i64 4)
// CHECK-128-NEXT:    ret <vscale x 2 x i1> [[TMP1]]
//
fixed_bool32_t call_bool32_fs(fixed_bool32_t op1, vbool32_t op2) {
  return __riscv_vmand(op1, op2, __riscv_v_fixed_vlen / 32);
}

// CHECK-64-LABEL: @call_bool64_fs(
// CHECK-64-NEXT:  entry:
// CHECK-64-NEXT:    [[TMP1:%.*]] = tail call <vscale x 1 x i1> @llvm.riscv.vmand.nxv1i1.i64(<vscale x 1 x i1> [[TMP0:%.*]], <vscale x 1 x i1> [[OP2:%.*]], i64 1)
// CHECK-64-NEXT:    ret <vscale x 1 x i1> [[TMP1]]
//
// CHECK-128-LABEL: @call_bool64_fs(
// CHECK-128-NEXT:  entry:
// CHECK-128-NEXT:    [[TMP1:%.*]] = tail call <vscale x 1 x i1> @llvm.riscv.vmand.nxv1i1.i64(<vscale x 1 x i1> [[TMP0:%.*]], <vscale x 1 x i1> [[OP2:%.*]], i64 2)
// CHECK-128-NEXT:    ret <vscale x 1 x i1> [[TMP1]]
//
fixed_bool64_t call_bool64_fs(fixed_bool64_t op1, vbool64_t op2) {
  return __riscv_vmand(op1, op2, __riscv_v_fixed_vlen / 64);
}

//===----------------------------------------------------------------------===//
// scalable, scalable
//===----------------------------------------------------------------------===//

// CHECK-64-LABEL: @call_bool32_ss(
// CHECK-64-NEXT:  entry:
// CHECK-64-NEXT:    [[TMP0:%.*]] = tail call <vscale x 2 x i1> @llvm.riscv.vmand.nxv2i1.i64(<vscale x 2 x i1> [[OP1:%.*]], <vscale x 2 x i1> [[OP2:%.*]], i64 2)
// CHECK-64-NEXT:    ret <vscale x 2 x i1> [[TMP0]]
//
// CHECK-128-LABEL: @call_bool32_ss(
// CHECK-128-NEXT:  entry:
// CHECK-128-NEXT:    [[TMP0:%.*]] = tail call <vscale x 2 x i1> @llvm.riscv.vmand.nxv2i1.i64(<vscale x 2 x i1> [[OP1:%.*]], <vscale x 2 x i1> [[OP2:%.*]], i64 4)
// CHECK-128-NEXT:    ret <vscale x 2 x i1> [[TMP0]]
//
fixed_bool32_t call_bool32_ss(vbool32_t op1, vbool32_t op2) {
  return __riscv_vmand(op1, op2, __riscv_v_fixed_vlen / 32);
}

// CHECK-64-LABEL: @call_bool64_ss(
// CHECK-64-NEXT:  entry:
// CHECK-64-NEXT:    [[TMP0:%.*]] = tail call <vscale x 1 x i1> @llvm.riscv.vmand.nxv1i1.i64(<vscale x 1 x i1> [[OP1:%.*]], <vscale x 1 x i1> [[OP2:%.*]], i64 1)
// CHECK-64-NEXT:    ret <vscale x 1 x i1> [[TMP0]]
//
// CHECK-128-LABEL: @call_bool64_ss(
// CHECK-128-NEXT:  entry:
// CHECK-128-NEXT:    [[TMP0:%.*]] = tail call <vscale x 1 x i1> @llvm.riscv.vmand.nxv1i1.i64(<vscale x 1 x i1> [[OP1:%.*]], <vscale x 1 x i1> [[OP2:%.*]], i64 2)
// CHECK-128-NEXT:    ret <vscale x 1 x i1> [[TMP0]]
//
fixed_bool64_t call_bool64_ss(vbool64_t op1, vbool64_t op2) {
  return __riscv_vmand(op1, op2, __riscv_v_fixed_vlen / 64);
}
