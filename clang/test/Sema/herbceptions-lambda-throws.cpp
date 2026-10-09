// RUN: not %clang_cc1 -std=c++26 -fherbceptions -emit-obj -o /dev/null %s 2>&1 | FileCheck %s

// Bare calls to throws functions inside a lambda that is itself not declared
// 'throws' must be rejected (the user can mark the lambda 'throws' to opt in,
// same as a free function). Previously this triggered an ICE in CodeGen when
// Sema auto-wrapped the call in a CXXTryExpr using the enclosing function's
// 'throws' spec, but the lambda's call operator has no such spec.

inline constexpr void inner() throws {}

inline constexpr void caller() {
  // CHECK: error: call to 'throws' function in a non-'throws' function must be handled by an enclosing 'try { } catch throws' block, or mark the calling function as 'throws' so herbceptions can propagate
  auto lam = [] { inner(); };
  lam();
}

// Same for a lambda with an explicit '()' parameter list.
inline constexpr void caller2() {
  // CHECK: error: call to 'throws' function in a non-'throws' function must be handled by an enclosing 'try { } catch throws' block, or mark the calling function as 'throws' so herbceptions can propagate
  auto lam = []() { inner(); };
  lam();
}

// A lambda explicitly marked 'throws' IS allowed to call a throws function;
// the call is auto-propagated just as in a free throws function.
inline constexpr void caller3() throws {
  auto lam = []() throws { inner(); };
  lam();
}

// A lambda marked 'throws' inside a function that is NOT 'throws' is also
// allowed: the lambda's own spec governs what its body may do, and the call
// site is responsible for handling the error (e.g. with a try/catch throws
// block). Previously getCurFunctionDecl() skipped the lambda context and
// the enclosing function's missing spec caused a spurious rejection.
inline constexpr void caller4() {
  auto lam = []() throws { inner(); };
  try {
    lam();
  } catch throws(int) {
  }
}

// `throw throws` inside a throws lambda nested in a non-throws function.
inline constexpr void caller5() {
  auto lam = []() throws { throw throws 0; };
  try {
    lam();
  } catch throws(int) {
  }
}

int main() {
  caller();
  caller2();
  caller3();
  return 0;
}
