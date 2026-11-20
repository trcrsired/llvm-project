// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// `[[unsafe]] T x = ...;` covers the initializer expression, not a lambda
// body nested inside it. A lambda body is checked as safe source; unsafe
// operations there need their own [[unsafe]] fence.

[[unsafe]] {
struct V {
  int x;
  [[unsafe]] V(int) noexcept {}
  void use() noexcept {}
};
}

static void good() noexcept {
  [[unsafe]] V v(1);              // unsafe ctor in the initializer: OK
  v.use();
  [[unsafe]] auto fn = [] {
    [[unsafe]] { V inner(2); }    // explicit fence inside the body: OK
  };
  fn();
}

static void bad() noexcept {
  [[unsafe]] auto fn = [] {
    V v(1); // expected-error {{call to 'V' is unsafe}}
    v.use();
  };
  fn();
}

// A lambda defined inside a real [[unsafe]] block is still unchecked.
static void fenced() noexcept {
  [[unsafe]] {
    auto fn = [] { V v(3); v.use(); };
    fn();
  }
}
