// RUN: %clang_cc1 -fsyntax-only -fsafecxx -Wno-return-stack-address -verify %s
// RUN: %clang_cc1 -fsyntax-only -fparameter-passing -flifetime-protocol -Wno-return-stack-address -verify %s
//
// D4 dangling: `in auto b = <temporary owner>[i];` is a borrow of a value
// whose owner dies at the end of the full-expression. The P1179 engine must
// diagnose the later use. This is exactly why in/inout auto lower to tracked
// references rather than by-value copies.

static void sink(in int) noexcept;

[[unsafe]] {
struct V {
  int a[4];
  const int &operator[](int i) const noexcept
      [[clang::lifetimebound]] { return a[i]; }
};
V make() noexcept { return V{}; }
}

static void bad() noexcept {
  in auto b = make()[1]; // expected-warning {{temporary bound to local reference}} expected-error {{temporary object does not live long enough}} expected-note {{temporary object is destroyed here}} expected-note {{aliases the storage of temporary object}}
  sink(b); // expected-note {{later used here}}
}

// A borrow of an owner that outlives the call is fine.
static void good(in V v) noexcept {
  in auto b = v[1];
  sink(b);
}
