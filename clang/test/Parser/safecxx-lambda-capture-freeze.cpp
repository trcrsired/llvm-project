// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// Calling a lambda is a use of the variables it captured. A lambda that
// captures a range-for owner may not be called while that owner is frozen,
// even if the lambda was defined before the loop.

static void use_obj(in int) noexcept {}

[[unsafe]] {
struct It {
  int x;
  int &operator*() noexcept { return x; }
  It &operator++() noexcept { return *this; }
  bool operator!=(It const &) const noexcept { return false; }
};
struct V {
  [[unsafe]] It begin() noexcept { return {}; }
  [[unsafe]] It end() noexcept { return {}; }
};
}

static void use_v(in V) noexcept {}

static void direct_use(inout V v) noexcept {
  for (in auto x : v) {
    (void)x;
    use_v(v); // expected-error {{cannot use 'v' inside a range-for over it}}
  }
}

static void captured_use(inout V v) noexcept {
  auto lam = [&]() { use_v(v); };
  for (in auto x : v) {
    (void)x;
    lam(); // expected-error {{cannot use 'v' inside a range-for over it}}
  }
}

static void immediate_use(inout V v) noexcept {
  for (in auto x : v) {
    (void)x;
    [&]() { use_v(v); }(); // expected-error {{cannot use 'v' inside a range-for over it}} expected-error {{cannot use 'v' inside a range-for over it}} expected-error {{cannot use 'v' inside a range-for over it}}
  }
}

static void outside_loop(inout V v, in int n) noexcept {
  auto lam = [&]() { use_v(v); };
  for (in auto x : v) {
    (void)x;
    (void)n;
  }
  lam(); // OK: the freeze has ended
}

static void unrelated_capture(inout V v) noexcept {
  int n = 0;
  auto lam = [&]() { use_obj(n); };
  for (in auto x : v) {
    (void)x;
    lam(); // OK: captures an unrelated variable
  }
}
