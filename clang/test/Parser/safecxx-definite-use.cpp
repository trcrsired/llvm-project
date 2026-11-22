// RUN: %clang_cc1 -fsyntax-only -fparameter-passing -verify %s

// d0708 definite first use: an `out` parameter must be constructed on every
// path before it is read, and on every path that leaves the function. The
// check is a must-analysis over the CFG, so a write in one branch does not
// satisfy a read on the other branch.

static void use(in int) noexcept {}
static void fill(out int v) noexcept { v = 0; }

static void bad_read(out int x) noexcept {
  int y = x; // expected-error {{out parameter 'x' is read before it is constructed}}
  x = 1;
  (void)y;
}

static void bad_never(out int x) noexcept { // expected-error {{out parameter 'x' is never constructed}}
}

static void good(out int x) noexcept {
  x = 1;
  int y = x; // OK: read after write
  (void)y;
}

// A conditional write does not construct the parameter on the other path.
static void branch_read(out int x, bool c) noexcept {
  if (c) x = 1;
  use(x); // expected-error {{out parameter 'x' is read before it is constructed}}
}

// A conditional write is also not a construction on every path out.
static void branch_only(out int x, bool c) noexcept { // expected-error {{out parameter 'x' is never constructed}}
  if (c) x = 1;
}

// Both branches construct: fine.
static void both_branches(out int x, bool c) noexcept {
  if (c) x = 1;
  else x = 2;
  use(x); // OK
}

// A write later in the loop body does not cover the first iteration's read.
static void loop_read_first(out int x) noexcept {
  for (int i = 0; i < 2; ++i) {
    if (i == 1) use(x); // expected-error {{out parameter 'x' is read before it is constructed}}
    else x = 1;
  }
}

// Passing to an `out` parameter constructs the argument.
static void call_constructs(out int x) noexcept {
  fill(x);
  use(x); // OK
}

// A compound assignment reads its target first.
static void compound(out int x) noexcept {
  x += 1; // expected-error {{out parameter 'x' is read before it is constructed}}
  use(x);
}

// An unevaluated operand is not a read.
static void unevaluated(out int x) noexcept {
  x = 1;
  (void)sizeof(x); // OK
}

// Ordinary parameters are unaffected.
static void ordinary(int x) noexcept {
  int y = x;
  (void)y;
}

// The check is off without -fparameter-passing (see the disabled test).
