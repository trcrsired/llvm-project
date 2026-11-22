// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx -verify %s

// A structured binding that names an [[unsafe]] data member is itself unsafe.
// Creating the binding names the member, so an unmarked decomposition is
// rejected at the declaration. A [[unsafe]] mark on the declaration exempts
// the binding creation, but each binding that aliases an unsafe member must be
// fenced at every use.

struct S {
  [[unsafe]] int secret = 0;
  int pub = 1;
};

static void direct(inout S s) noexcept {
  (void)s.secret; // expected-error {{data member 'secret' is declared [[unsafe]]}}
}

// Unmarked: naming secret is a use without a fence.
static void unmarked(inout S s) noexcept {
  auto& [a, b] = s; // expected-error {{structured binding names data member 'secret' declared [[unsafe]]}}
  (void)b;
}

// Marked declaration: binding creation is fine, but a is unsafe afterward.
static void marked(inout S s) noexcept {
  [[unsafe]] auto& [a, b] = s;
  (void)b;                 // OK: pub is not unsafe
  a = 5;                   // expected-error {{binding 'a' names a data member declared [[unsafe]]}}
  (void)a;                 // expected-error {{binding 'a' names a data member declared [[unsafe]]}}
  [[unsafe]] { a = 6; (void)a; } // OK: fenced use
}

// A fence around the whole decomposition needs no mark: the bindings go out
// of scope with the fence.
static void fenced(inout S s) noexcept {
  [[unsafe]] {
    auto& [a, b] = s;
    a = 7;
    (void)a;
  }
}

// A class with no unsafe member is unaffected.
struct Safe { int x = 1; int y = 2; };
static void safe(inout Safe s) noexcept {
  auto& [a, b] = s;
  a = 3;
  (void)b;
}
