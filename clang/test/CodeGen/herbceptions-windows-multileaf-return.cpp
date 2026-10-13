// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++26 -fherbceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=MSVC
// RUN: %clang_cc1 -triple x86_64-w64-windows-gnu -std=c++26 -fherbceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=GNU
// RUN: %clang_cc1 -triple x86_64-pc-cygwin -std=c++26 -fherbceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=GNU
// RUN: %clang_cc1 -triple x86_64-pc-msys -std=c++26 -fherbceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=GNU
// RUN: %clang_cc1 -triple x86_64-unknown-uefi -std=c++26 -fherbceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=MSVC

// Herbceptions on x86-64 Win64-family targets (Windows, MinGW, Cygwin, MSYS,
// UEFI): a trivially-copyable <=16-byte payload is returned as two i64 register
// leaves plus the i1 discriminant, regardless of the source type's field/array
// shape.
//
// A type whose *raw* LLVM type would decompose into more leaves than the
// return-register budget (e.g. { [8 x i8], i64 } or { i32, i32, i32, i32 })
// has to be coerced to { i64, i64 }; returning the raw type used to abort in
// CCState::AnalyzeReturn with "unable to allocate function return #3".

struct empty {};
// The empty member and the padding before x make the raw LLVM type
// { [8 x i8], i64 }.
struct S {
  empty e;
  unsigned long long x;
};
struct FourInts {
  int a, b, c, d;
};

// MSVC-LABEL: define {{.*}} { { i64, i64 }, i1 } @"?f@@YA?AUS@@_K@_FH"(i64 noundef %v)
// GNU-LABEL: define {{.*}} { { i64, i64 }, i1 } @_Z1fDEiEy(i64 noundef %v)
S f(unsigned long long v) return_failure{int} { return S{empty{}, v}; }

// MSVC-LABEL: define {{.*}} { { i64, i64 }, i1 } @"?g@@YA?AUFourInts@@H@_FH"(i32 noundef %v)
// GNU-LABEL: define {{.*}} { { i64, i64 }, i1 } @_Z1gDEiEi(i32 noundef %v)
FourInts g(int v) return_failure{int} { return FourInts{v, v, v, v}; }
