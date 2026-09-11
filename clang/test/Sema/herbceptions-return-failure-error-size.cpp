// RUN: not %clang_cc1 -std=c++26 -fherbceptions -fcxx-exceptions -fsyntax-only %s 2>&1 | FileCheck %s
// RUN: not %clang_cc1 -std=c++26 -fherbceptions -fcxx-exceptions -fsyntax-only -triple i686-linux-gnu %s 2>&1 | FileCheck %s --check-prefix=CHECK32

// The 'return_failure{E}' error type travels in the same registers as the
// failure discriminant, so it is bounded by 2 * sizeof(uintptr_t) -- the room
// the implicit 'throws' error type occupies (std::error is {void *, uintptr_t}).
// A wider error could not ride in registers, so it is rejected here in the
// front end rather than failing later during code generation. The bound is a
// property of the target, not of the source, so it follows the pointer width:
// 16 bytes on a 64-bit target, 8 bytes on a 32-bit one.

struct E8  { unsigned long long a; };
struct E16 { void *p; unsigned long long n; };
struct E24 { unsigned long long a, b, c; };

// Within budget on every target.
int ok8() return_failure{E8};

// Exactly the budget on a 64-bit target, twice it on a 32-bit one. The two runs
// therefore disagree: the 64-bit run reports only the E24 error below, while
// the 32-bit run reports both.
int maybe16() return_failure{E16};

// CHECK: error: the 'return_failure{...}' error type 'E24' may not be larger than 2 * sizeof(uintptr_t) (16 bytes)
// CHECK: 1 error generated.
int bad24() return_failure{E24};

// CHECK32: error: the 'return_failure{...}' error type 'E16' may not be larger than 2 * sizeof(uintptr_t) (8 bytes)
// CHECK32: error: the 'return_failure{...}' error type 'E24' may not be larger than 2 * sizeof(uintptr_t) (8 bytes)
// CHECK32: 2 errors generated.
