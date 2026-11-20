// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// `[[unsafe]] T x = ...;` marks the initialization as an unsafe region. The
// variable itself is an ordinary safe object, usable in safe source
// afterward. This is the leading-attribute form; there is no trailing one.

[[unsafe]] {
struct V {
  int x;
  [[unsafe]] V(int) noexcept {}
  void use() noexcept {}
};
}

static void good() noexcept {
  [[unsafe]] V v(1);  // unsafe constructor allowed here
  v.use();            // safe object afterward
  [[unsafe]] V w = V(2);
  w.use();
  [[unsafe]] V u{3};
  u.use();
}

static void bad() noexcept {
  V v(1); // expected-error {{call to 'V' is unsafe}}
  (void)v;
}
