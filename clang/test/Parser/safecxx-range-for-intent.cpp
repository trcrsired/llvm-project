// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s
// RUN: %clang_cc1 -fsyntax-only -fparameter-passing %s
//
// d0708 §1.4: a range-for loop variable is a parameter and must declare its
// passing intent. Bare `auto` (a silent copy) is banned in safe source; the
// replacement spellings are in auto / inout auto. Fences and non-safe modes
// are unaffected.

static void sink(in int) noexcept {}
[[unsafe]] {
struct V {
  int a[4];
  int *begin() noexcept { return a; }
  int *end() noexcept { return a + 4; }
};
}

static void bad_auto(inout V v) noexcept {
  // expected-error@+1 {{range-for loop variable must declare its passing intent}}
  for (auto x : v) sink(x);
}

static void bad_explicit_type(inout V v) noexcept {
  // expected-error@+1 {{range-for loop variable must declare its passing intent}}
  for (int x : v) sink(x);
}

static void bad_ref(inout V v) noexcept {
  // expected-error@+1 {{raw pointer or reference type}} expected-error@+1 {{range-for loop variable must declare its passing intent}}
  for (auto &x : v) sink(x);
}

static void good(inout V v) noexcept {
  for (in auto x : v) sink(x);
  for (inout auto y : v) y = 1;
  for (in int z : v) sink(z);
}

static void fenced(inout V v) noexcept {
  [[unsafe]] {
    for (auto x : v) sink(x);
  }
}
