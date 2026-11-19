// RUN: %clang_cc1 -fsyntax-only -fparameter-passing -verify %s

// d0708 definite-last-use: passing a local as the argument of a
// `move`/`destructive_move` parameter consumes it; any later use is diagnosed.

struct T { int x; };

static void by_move(move T v) noexcept {}
static void by_dmove(destructive_move T v) noexcept {}

static void good() noexcept {
  T a;
  by_move(static_cast<T &&>(a)); // last use of a
}

static void bad_move() noexcept {
  T a;
  by_move(static_cast<T &&>(a));
  T b = a; // expected-error {{value 'a' is used after it was passed to a move parameter}}
  (void)b;
}

static void bad_dmove() noexcept {
  T a;
  by_dmove(static_cast<T &&>(a));
  a.x = 1; // expected-error {{value 'a' is used after it was passed to a move parameter}}
}

// Use before the move is fine.
static void before() noexcept {
  T a;
  a.x = 1;
  by_move(static_cast<T &&>(a));
}

// Ordinary parameters and non-move calls are unaffected.
static void ordinary(int p) noexcept {
  int q = p;
  (void)q;
}
