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
