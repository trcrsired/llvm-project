// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fherbceptions -fclangir -emit-llvm -o - %s | FileCheck %s

// CIR counterpart of clang/test/CodeGen/herbceptions-mangling.cpp, Itanium
// only: CIR does not implement the MSVC C++ ABI yet, so only the Itanium
// mangling checks are ported.
//
// The throws/return_failure specifier is part of the mangled name, both for
// a function's own encoding and for function types in nested positions.
// Itanium encodes Dr / DE <type> E / Dg <expr> E in the <exception-spec>
// slot of a nested function type and between the name and the
// bare-function-type of a function's own encoding.

using Plain = int (*)();
using Throws = int (*)() throws;
using Noexcept = int (*)() noexcept;
using FailsInt = int (*)() return_failure{int};
using FailsLong = int (*)() return_failure{long};

// CHECK-DAG: define {{.*}}void @_Z4takePFivE(
// CHECK-DAG: define {{.*}}void @_Z4takePDrFivE(
// CHECK-DAG: define {{.*}}void @_Z4takePDoFivE(
void take(Plain) {}
void take(Throws) {}
void take(Noexcept) {}

// CHECK-DAG: define {{.*}}void @_Z6take_rPDEiEFivE(
// CHECK-DAG: define {{.*}}void @_Z6take_rPDElEFivE(
void take_r(FailsInt) {}
void take_r(FailsLong) {}

// The function's own encoding carries the specifier between the name and the
// bare-function-type. throws(false) is type-equivalent to noexcept and gets
// no marker.
// CHECK-DAG: define {{.*}}@_Z1fDrv(
// CHECK-DAG: define {{.*}}@_Z1gDEiEv(
// CHECK-DAG: define {{.*}}@_Z2nfv(
int f() throws { return 1; }
int g() return_failure{int} { return 2; }
int nf() throws(false) { return 3; }

// CHECK-DAG: define {{.*}}void @_ZN1SIPFivEE1mEv(
// CHECK-DAG: define {{.*}}void @_ZN1SIPDrFivEE1mEv(
// CHECK-DAG: define {{.*}}void @_ZN1SIPDEiEFivEE1mEv(
template <typename T> struct S { static void m() {} };
template struct S<Plain>;
template struct S<Throws>;
template struct S<FailsInt>;
