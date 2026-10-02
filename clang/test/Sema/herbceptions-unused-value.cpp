// RUN: %clang_cc1 -std=c++26 -fherbceptions -fsyntax-only -Wall -Wextra -verify %s

// A discarded 'throws' call must not warn merely because the implicit try()
// wrapper has a non-void type: whether a statement-level call warns is decided
// by the wrapped call itself (nodiscard/pure/const), exactly as for a call
// without a 'throws' spec.

namespace std {
struct error {
  void *d;
  __SIZE_TYPE__ c;
};
} // namespace std

int throwing() throws;
void throwing_void() throws;
[[nodiscard]] int throwing_nodiscard() throws;

struct S {
  int method() throws;
  [[nodiscard]] int method_nodiscard() throws;
};

void caller() throws {
  throwing();        // no warning expected
  throwing_void();   // no warning expected
  throwing_nodiscard(); // expected-warning {{ignoring return value of function declared with 'nodiscard' attribute}}

  S s;
  s.method();            // no warning expected
  s.method_nodiscard(); // expected-warning {{ignoring return value of function declared with 'nodiscard' attribute}}
}
