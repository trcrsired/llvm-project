// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// A write borrow (out/inout) may not name the same object as any other borrow
// argument of the same call. Two read-only in arguments may alias.

static void write_first(out int a, in int b) noexcept {
  a = 1;
  (void)b;
}
static void mutate(inout int a, in int b) noexcept {
  a = 2;
  (void)b;
}
static void read_both(in int a, in int b) noexcept {
  (void)a;
  (void)b;
}
static void write_both(out int a, out int b) noexcept {
  a = 1;
  b = 2;
}

static void f() noexcept {
  int x = 0;
  write_first(x, x); // expected-error {{the same object 'x' is passed to more than one borrow parameter}} expected-note {{first passed here as a out parameter}}
  mutate(x, x);      // expected-error {{the same object 'x' is passed to more than one borrow parameter}} expected-note {{first passed here as a inout parameter}}
  write_both(x, x);  // expected-error {{the same object 'x' is passed to more than one borrow parameter}} expected-note {{first passed here as a out parameter}}
  read_both(x, x);   // OK: both are read-only
}

static void different() noexcept {
  int x = 0, y = 0;
  write_first(x, y); // OK: distinct objects
}

// A fenced call is legacy code and is not checked.
static void fenced() noexcept {
  int x = 0;
  [[unsafe]] { write_first(x, x); }
}
