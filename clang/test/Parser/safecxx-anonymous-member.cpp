// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx -verify %s

// A member of an anonymous union/struct is found as an IndirectFieldDecl. The
// [[unsafe]] access restriction must look through it to the real FieldDecl, so
// grouping fields in an anonymous record is not a back door.

struct S {
  union { [[unsafe]] int secret; int pub; };
  struct { [[unsafe]] int hidden; int shown; };
};

static void read(inout S s) noexcept {
  (void)s.secret; // expected-error {{data member 'secret' is declared [[unsafe]]}}
  (void)s.hidden; // expected-error {{data member 'hidden' is declared [[unsafe]]}}
  (void)s.pub;    // OK
  (void)s.shown;  // OK
}

static void write(inout S s) noexcept {
  s.secret = 1; // expected-error {{data member 'secret' is declared [[unsafe]]}}
  s.hidden = 2; // expected-error {{data member 'hidden' is declared [[unsafe]]}}
}

static void fenced(inout S s) noexcept {
  [[unsafe]] {
    (void)s.secret;
    s.hidden = 3;
  }
}
