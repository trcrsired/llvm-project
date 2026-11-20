// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s
// RUN: %clang_cc1 -fsyntax-only -fparameter-passing -verify %s

// Call-site relocation intent. `destructive_move(x)` lowers to an rvalue cast;
// the callee's by-value destructive_move parameter is constructed from it.
// The definite-last-use check still sees through the marker.

struct Buf { int x; };

static void by_dmove(destructive_move Buf b) noexcept {}
static void plain_by_value(Buf b) noexcept {}

static void good() noexcept {
  Buf b{2};
  by_dmove(destructive_move(b));
}

static void bad() noexcept {
  Buf a{1};
  by_dmove(destructive_move(a));
  a.x = 2; // expected-error {{value 'a' is used after it was passed to a move parameter}}
}

// The marker itself ends the lifetime, even with an ordinary by-value callee.
static void plain_callee() noexcept {
  Buf a{1};
  plain_by_value(destructive_move(a));
  a.x = 2; // expected-error {{value 'a' is used after it was passed to a move parameter}}
}
