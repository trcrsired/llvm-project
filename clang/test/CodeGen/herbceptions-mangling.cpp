// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fherbceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=ITANIUM
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -fherbceptions -fms-compatibility-version=19.30 -emit-llvm -o - %s | FileCheck %s --check-prefix=MSVC

// The throws/return_failure specifier of a function *type* is mangled in
// nested positions (Itanium encodes Dr / DE <type> E in the <exception-spec>
// slot; MSVC encodes _H / _F <type> in the <throw-spec> slot). A function's
// own mangled name does not carry the specifier, matching noexcept.

using Plain = int (*)();
using Throws = int (*)() throws;
using Noexcept = int (*)() noexcept;
using FailsInt = int (*)() return_failure{int};
using FailsLong = int (*)() return_failure{long};

// ITANIUM-DAG: define {{.*}}void @_Z4takePFivE(
// ITANIUM-DAG: define {{.*}}void @_Z4takePDrFivE(
// ITANIUM-DAG: define {{.*}}void @_Z4takePDoFivE(
// MSVC-DAG: define {{.*}}void @"?take@@YAXP6AHXZ@Z"(
// MSVC-DAG: define {{.*}}void @"?take@@YAXP6AHX_H@Z"(
// MSVC-DAG: define {{.*}}void @"?take@@YAXP6AHX_E@Z"(
void take(Plain) {}
void take(Throws) {}
void take(Noexcept) {}

// ITANIUM-DAG: define {{.*}}void @_Z6take_rPDEiEFivE(
// ITANIUM-DAG: define {{.*}}void @_Z6take_rPDElEFivE(
// MSVC-DAG: define {{.*}}void @"?take_r@@YAXP6AHX_FH@Z"(
// MSVC-DAG: define {{.*}}void @"?take_r@@YAXP6AHX_FJ@Z"(
void take_r(FailsInt) {}
void take_r(FailsLong) {}

// Top-level function names do not carry the specifier.
// ITANIUM-DAG: define {{.*}}@_Z1fv(
// ITANIUM-DAG: define {{.*}}@_Z1gv(
// ITANIUM-DAG: define {{.*}}@_Z2nfv(
// MSVC-DAG: define {{.*}}@"?f@@YAHXZ"(
// MSVC-DAG: define {{.*}}@"?g@@YAHXZ"(
// MSVC-DAG: define {{.*}}@"?nf@@YAHXZ"(
int f() throws { return 1; }
int g() return_failure{int} { return 2; }
int nf() throws(false) { return 3; }

// ITANIUM-DAG: define {{.*}}void @_ZN1SIPFivEE1mEv(
// ITANIUM-DAG: define {{.*}}void @_ZN1SIPDrFivEE1mEv(
// ITANIUM-DAG: define {{.*}}void @_ZN1SIPDEiEFivEE1mEv(
// MSVC-DAG: define {{.*}}void @"?m@?$S@P6AHXZ@@SAXXZ"(
// MSVC-DAG: define {{.*}}void @"?m@?$S@P6AHX_H@@SAXXZ"(
// MSVC-DAG: define {{.*}}void @"?m@?$S@P6AHX_FH@@SAXXZ"(
template <typename T> struct S { static void m() {} };
template struct S<Plain>;
template struct S<Throws>;
template struct S<FailsInt>;
