// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// [[unsafe]] on a data member is an access restriction: the member may be
// named only from inside [[unsafe]] context. This covers explicit member
// access, implicit `this->member`, and ctor-initializers.

struct S {
  [[unsafe]] int secret = 0;
  int open = 0;

  int implicit_use() noexcept {
    return secret; // expected-error {{data member 'secret' is declared [[unsafe]]}}
  }
  int implicit_fenced() noexcept {
    [[unsafe]] { return secret; }
  }
};

static void read(in S s) noexcept {
  (void)s.open;
  (void)s.secret; // expected-error {{data member 'secret' is declared [[unsafe]]}}
}

static void write(inout S s) noexcept {
  s.open = 1;
  s.secret = 2; // expected-error {{data member 'secret' is declared [[unsafe]]}}
}

static void fenced(inout S s) noexcept {
  [[unsafe]] {
    (void)s.secret;
    s.secret = 2;
  }
}

struct H {
  [[unsafe]] S s;
  H() noexcept : s() {} // expected-error {{data member 's' is declared [[unsafe]]}}
};

// A static data member is a VarDecl, not a FieldDecl, but [[unsafe]] on it is
// still an access restriction. Both `S::x` (a DeclRefExpr) and `s.x` (a member
// reference) must be rejected in safe source.
struct T {
  [[unsafe]] static int hidden;
  static int visible;
};
int T::hidden = 0;
int T::visible = 0;

static void static_read(in T t) noexcept {
  (void)T::hidden; // expected-error {{data member 'hidden' is declared [[unsafe]]}}
  (void)T::visible;
  (void)t.hidden;  // expected-error {{data member 'hidden' is declared [[unsafe]]}}
  (void)t.visible;
}

static void static_fenced() noexcept {
  [[unsafe]] {
    (void)T::hidden;
    (void)T::visible;
  }
}
