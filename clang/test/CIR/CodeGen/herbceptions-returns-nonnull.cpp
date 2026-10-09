// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-cir -o - %s | FileCheck %s --check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm -o - %s | FileCheck %s --check-prefix=LLVM

// The LLVM return value of a throws/fails function is the error channel
// ({E, i1} aggregate or a lone i1 discriminant), not the C++ return value.
// A returns_nonnull annotation applies to that C++ value, so llvm.nonnull
// must not be stamped on the aggregate return — the LLVM dialect rejects
// it on non-pointer types.

extern "C" void *malloc(unsigned long);

// CIR-LABEL:  cir.func {{.*}} @_Z10alloc_likeDrm(
// CIR-SAME:   -> !rec_anon_struct1
// CIR-NOT:    llvm.nonnull
// LLVM-LABEL: define {{.*}} { { ptr, i64 }, i8 } @_Z10alloc_likeDrm(
// LLVM-NOT:   nonnull
// LLVM:       ret { { ptr, i64 }, i8 }
__attribute__((returns_nonnull)) void *alloc_like(unsigned long n) throws {
  return malloc(n);
}

// Sanity: a plain function still gets the attribute.
// CIR-LABEL:  cir.func {{.*}} @_Z11alloc_plainm(
// CIR-SAME:   -> (!cir.ptr<!void> {llvm.nonnull
// LLVM-LABEL: define {{.*}}nonnull{{.*}} ptr @_Z11alloc_plainm(
__attribute__((returns_nonnull)) void *alloc_plain(unsigned long n) {
  return malloc(n);
}
