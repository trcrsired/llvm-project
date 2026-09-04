// RUN: %clang_cc1 -std=c++20 -fherbceptions -fsyntax-only -verify %s

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
};
}

// A deterministic handler is a CXXCatchThrowsStmt, not a CXXCatchStmt. Every
// generic CXXTryStmt consumer must branch on that invariant before using the
// traditional-only getCatchHandler accessor.
void reject_jump_into_handler() throws {
  goto inside; // expected-error {{cannot jump from this goto statement to its label}}
  try {
  } catch throws(std::error error) { // expected-note {{jump bypasses initialization of catch block}}
  inside:
    (void)error;
  }
}

struct constructor_handler {
  constructor_handler() try {
  } catch throws(std::error error) {
    (void)error;
    return; // expected-error {{return in the catch of a function try block of a constructor is illegal}}
  }
};
