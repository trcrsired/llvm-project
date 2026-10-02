// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm %s -o - | FileCheck %s

// Verify that T& / T&& returns through `throws` preserve the underlying
// value-kind so the caller's `return inner_throws();` does not collapse
// the reference into a temporary. The throws ABI stores the success
// pointer in the payload slot; the caller's auto-propagation must hand
// through the original pointer without copying the referent.
//
// Also verify T* returns share the same calling convention as T&/T&&:
// all pointer-typed throws returns collapse to {{ptr, i64}, i8} in CIR.

namespace std {
struct error { void *d; __SIZE_TYPE__ c; ~error() noexcept; };
}

int g = 42;

// CHECK-LABEL: define {{.*}} { { ptr, i64 }, i8 } @_Z11lref_middleDrv(
int& lref_inner() { return g; }
int& lref_middle() throws { return lref_inner(); }

// CHECK-LABEL: define {{.*}} { { ptr, i64 }, i8 } @_Z10lref_outerDrv(
// CHECK: call { { ptr, i64 }, i8 } @_Z11lref_middleDrv()
int& lref_outer() throws { return lref_middle(); }

// CHECK-LABEL: define {{.*}} { { ptr, i64 }, i8 } @_Z11rref_middleDrv(
int&& rref_inner() { return static_cast<int&&>(g); }
int&& rref_middle() throws { return rref_inner(); }

// CHECK-LABEL: define {{.*}} { { ptr, i64 }, i8 } @_Z10rref_outerDrv(
int&& rref_outer() throws { return rref_middle(); }

// CHECK-LABEL: define {{.*}} { { ptr, i64 }, i8 } @_Z11cref_middleDrv(
const int& cref_inner() { return g; }
const int& cref_middle() throws { return cref_inner(); }

// CHECK-LABEL: define {{.*}} { { ptr, i64 }, i8 } @_Z10cref_outerDrv(
const int& cref_outer() throws { return cref_middle(); }

// CHECK-LABEL: define {{.*}} { { ptr, i64 }, i8 } @_Z10ptr_middleDrv(
int* ptr_inner() { return &g; }
int* ptr_middle() throws { return ptr_inner(); }

// CHECK-LABEL: define {{.*}} { { ptr, i64 }, i8 } @_Z9ptr_outerDrv(
int* ptr_outer() throws { return ptr_middle(); }
