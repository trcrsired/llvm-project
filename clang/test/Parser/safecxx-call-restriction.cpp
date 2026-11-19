// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s
// RUN: %clang_cc1 -fsyntax-only -fsafecxx-unsafe %s

// [[unsafe]] on a function is a call restriction: the function may only be
// called from inside an [[unsafe]] { } block. It says nothing about the
// function's own body. The restriction is a source-level rule of the umbrella
// only; -fsafecxx-unsafe (fence-only) does not enforce it.

[[unsafe]] static void unsafe_fn() noexcept {}

static void caller() noexcept {
  unsafe_fn(); // expected-error {{call to 'unsafe_fn' is unsafe}}

  [[unsafe]] {
    unsafe_fn(); // OK inside the fence
  }
}

// A namespace-scope fence covers calls in function bodies it contains.
[[unsafe]] {
  static void in_fence() noexcept {
    unsafe_fn(); // OK
  }
}

// Calls from an unfenced definition are restricted even if the definition sits
// after a fence.
static void after_fence() noexcept {
  unsafe_fn(); // expected-error {{call to 'unsafe_fn' is unsafe}}
}
