// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s
// RUN: %clang_cc1 -fsyntax-only -fparameter-passing -verify %s

// Call-site pass-intent markers. `move(x)` / `destructive_move(x)` lower to an
// rvalue cast; the callee's parameter kind decides T&& vs relocated by-value T.
// The definite-last-use check still sees through the marker.

struct Buf { int x; };

static void by_move(move Buf b) noexcept {}
static void by_dmove(destructive_move Buf b) noexcept {}

static void good() noexcept {
  Buf a{1};
  by_move(move(a));
  Buf b{2};
  by_dmove(destructive_move(b));
}

static void bad() noexcept {
  Buf a{1};
  by_dmove(destructive_move(a));
  a.x = 2; // expected-error {{value 'a' is used after it was passed to a move parameter}}
}
