// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// Iterator APIs are call-restricted when written in source, but a range-for's
// compiler-generated begin()/end() calls are exempt: the user never names the
// iterator and the loop variable is a checked in/inout borrow.

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

  // OK: compiler-generated begin()/end(), no iterator escapes.
  for (in auto x : r)
    (void)x;

  // Source-written calls are still restricted.
  (void)r.begin(); // expected-error {{call to 'begin' is unsafe}}
  (void)r.end();   // expected-error {{call to 'end' is unsafe}}
}

static void fenced_use(inout R r) noexcept {
  [[unsafe]] {
    for (in auto x : r)
      (void)x;
    (void)r.begin();
  }
}
