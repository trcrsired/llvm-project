// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// Use-after-move is flow-sensitive, not source-order sensitive: a use that
// appears before a move in the text can still follow it at runtime through a
// loop back-edge or a branch.

struct Buf { int x; };
static void consume(destructive_move Buf b) noexcept {}

static void loop() noexcept {
  Buf a{1};
  for (int i = 0; i < 2; ++i) {
    if (i == 1) a.x = 2; // expected-error {{value 'a' is used after it was passed to a move parameter}}
    else consume(destructive_move(a));
  }
}

static void branch() noexcept {
  Buf a{1};
  bool c = false;
  if (c)
    a.x = 1; // OK on this path: no move precedes it
  else
    consume(destructive_move(a));
}

static void before_move() noexcept {
  Buf a{1};
  a.x = 2; // OK: strictly before the move
  consume(destructive_move(a));
}

static void no_move() noexcept {
  Buf a{1};
  for (int i = 0; i < 2; ++i)
    a.x = i;
}

// A declaration starts a new lifetime. A loop-local variable moved at the end
// of an iteration is a fresh object on the next one, so the state must be
// killed at its declaration. The same loop with the variable declared outside
// still reports the use on the second iteration.
static void loop_local() noexcept {
  for (int i = 0; i < 3; ++i) {
    Buf local{1};
    local.x = i;                   // OK: fresh object each iteration
    consume(destructive_move(local));
  }
}

static void loop_outer() noexcept {
  Buf outer{1};
  for (int i = 0; i < 3; ++i) {
    outer.x = i; // expected-error {{value 'outer' is used after it was passed to a move parameter}}
    consume(destructive_move(outer));
  }
}
