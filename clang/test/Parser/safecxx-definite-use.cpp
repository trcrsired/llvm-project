// RUN: %clang_cc1 -fsyntax-only -fparameter-passing -verify %s

// d0708 definite first use: an `out` parameter must be assigned before it is
// read, and must be assigned at all.

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

// Ordinary parameters are unaffected.
static void ordinary(int x) noexcept {
  int y = x;
  (void)y;
}

// The check is off without -fparameter-passing (see the disabled test).
