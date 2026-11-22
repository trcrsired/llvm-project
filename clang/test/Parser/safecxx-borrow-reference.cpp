// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx -verify %s

// d0708 borrow keywords already imply a reference. An explicit '&' on the
// written type is redundant and rejected; a pointer operand is allowed.

struct S { int x = 1; int y = 2; };

// Parameters.
static void bad_in(in int &v) noexcept { // expected-error {{borrow already means a reference}}
  (void)v;
}
static void bad_inout(inout int &v) noexcept { // expected-error {{borrow already means a reference}}
  v = 1;
}
static void bad_out(out int &v) noexcept { // expected-error {{borrow already means a reference}}
  v = 1;
}
static void bad_move(move int &v) noexcept { // expected-error {{borrow already means a reference}}
  (void)v;
}
static void good_in(in int v) noexcept { (void)v; }
static void good_inout(inout int v) noexcept { v = 1; }
static void good_ptr(in int *p) noexcept { (void)p; }

// Variables.
static void vars() noexcept {
  int x = 1;
  in int &r = x; // expected-error {{borrow already means a reference}}
  (void)r;
  in int *p = &x; // OK: pointer operand
  (void)p;
}

// Structured bindings.
static void bindings() noexcept {
  S s;
  inout auto &[a, b] = s; // expected-error {{borrow already means a reference}}
  (void)a; (void)b;
  auto &[c, d] = s; // OK: no borrow keyword
  (void)c; (void)d;
}
