// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// Iterator APIs are call-restricted; index APIs are not. Range-for builds
// begin()/end() directly (bypassing ActOnCallExpr), so it must enforce the
// restriction too.

[[unsafe]] {
struct R {
  int *p = nullptr;
  [[unsafe]] int *begin() noexcept { return p; }
  [[unsafe]] int *end() noexcept { return p; }
  int &operator[](unsigned long) noexcept { return *p; }
};
}

static void use(inout R r) noexcept {
  r[0]; // OK: checked index API stays callable from safe code

  for (in auto x : r) // expected-error 2 {{is unsafe: it is declared [[unsafe]]}}
    (void)x;
}

static void fenced_use(inout R r) noexcept {
  [[unsafe]] {
    for (in auto x : r)
      (void)x;
    (void)r.begin();
  }
}
