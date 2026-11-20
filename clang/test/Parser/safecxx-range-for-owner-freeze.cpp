// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// A range-for over an owner/view freezes the range object for the duration of
// the loop body. Touching the range object in safe source is an error; the
// loop itself stays safe (its begin()/end() calls are compiler-generated).

[[unsafe]] {
struct V {
  int *p = nullptr;
  [[unsafe]] int *begin() noexcept { return p; }
  [[unsafe]] int *end() noexcept { return p; }
  int size() const noexcept { return 0; }
};

struct [[owner]] W {
  int *p = nullptr;
  int *begin() noexcept { return p; }
  int *end() noexcept { return p; }
};
}

static void use(in V v) noexcept {}
static void use_w(in W w) noexcept {}

static void unsafe_iterators(inout V v) noexcept {
  for (in auto x : v) {
    use(v);          // expected-error {{cannot use 'v' inside a range-for over it}}
    (void)v.size();  // expected-error {{cannot use 'v' inside a range-for over it}}
    (void)x;
  }
  use(v);            // OK: loop has ended
}

static void owned_type(inout W w) noexcept {
  for (in auto x : w) {
    use_w(w);        // expected-error {{cannot use 'w' inside a range-for over it}}
    (void)x;
  }
}

static void fenced(inout V v) noexcept {
  [[unsafe]] {
    for (in auto x : v) {
      use(v);
      (void)v.size();
      (void)x;
    }
  }
}
