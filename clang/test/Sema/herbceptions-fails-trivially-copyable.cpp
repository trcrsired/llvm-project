// RUN: not %clang_cc1 -std=c++26 -fherbceptions -fcxx-exceptions -fsyntax-only %s 2>&1 | FileCheck %s

// The 'return_failure{E}' error type must be trivially copyable, matching C behavior:
// the error value flows through the {T, i1} ABI slot by value. Non-trivially
// copyable error types (classes with user-defined copy/move/destructor) are
// rejected. 'return_failure{E} noexcept(false)' additionally allows traditional C++
// exceptions to propagate; the default 'return_failure{E}' implies noexcept(true).

struct NotTriviallyCopyable {
  NotTriviallyCopyable(const NotTriviallyCopyable&);
  ~NotTriviallyCopyable();
};

// CHECK: error: the 'return_failure{...}' error type 'NotTriviallyCopyable' must be trivially copyable
int bad() return_failure{NotTriviallyCopyable}; // expected-error {{the 'return_failure{...}' error type 'NotTriviallyCopyable' must be trivially copyable}}

// CHECK: error: the 'return_failure{...}' error type 'NotTriviallyCopyable' must be trivially copyable
int bad2() noexcept(false) return_failure{NotTriviallyCopyable}; // expected-error {{the 'return_failure{...}' error type 'NotTriviallyCopyable' must be trivially copyable}}

// Trivially copyable error types are accepted in every combination.
int ok1() return_failure{int};
int ok2() return_failure{int} noexcept(false);
int ok3() noexcept(false) return_failure{int};
int ok4() return_failure{int} noexcept(true);
