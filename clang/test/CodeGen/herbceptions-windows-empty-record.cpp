// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++26 -fherbceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=MSVC
// RUN: %clang_cc1 -triple x86_64-w64-windows-gnu -std=c++26 -fherbceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=GNU
// RUN: %clang_cc1 -triple x86_64-pc-cygwin -std=c++26 -fherbceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=GNU
// RUN: %clang_cc1 -triple x86_64-pc-msys -std=c++26 -fherbceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=GNU
// RUN: %clang_cc1 -triple x86_64-unknown-uefi -std=c++26 -fherbceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=MSVC

// Herbceptions on x86-64 Win64-family targets (Windows, MinGW, Cygwin, MSYS,
// UEFI): an empty C++ record passed by value to a 'throws' function consumes no
// register slot, so the following integer argument becomes the first one.  The
// plain MS x64 ABI still passes an empty record as a 1-byte value (i8), which
// is correct and must not change.
//
// The backend contract is "empty structs decompose to zero leaves"
// (X86ISelLoweringCall.cpp); the frontend must therefore not coerce the empty
// record to an i8 argument (which would eat RCX).

struct empty {};
struct errc {
  int e;
};

// throws: the empty record takes no slot, so 'c' is the only integer argument.
// MSVC-LABEL: define {{.*}} @"?f@@YAHUempty@@H@_FUerrc@@"(i32 noundef %c)
// GNU-LABEL: define {{.*}} @_Z1fDE4errcE5emptyi(i32 noundef %c)
int f(empty e, int c) return_failure{errc} { return c; }

// Plain MS x64 ABI: keep the empty record's i8 parameter (it takes RCX).
// MSVC-LABEL: define {{.*}} @"?g@@YAHUempty@@H@Z"(i8 %e.coerce, i32 noundef %c)
// GNU-LABEL: define {{.*}} @_Z1g5emptyi(i8 %e.coerce, i32 noundef %c)
int g(empty e, int c) { return c; }
