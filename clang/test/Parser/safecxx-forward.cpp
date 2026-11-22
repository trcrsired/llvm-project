// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx -verify %s

// D0708 draft 32: `forward X x` is the "pass along" parameter kind. It lowers
// to X& and preserves the argument's cv-qualification and value category at
// each definite last use (only the reference form is modeled here). It is a
// contextual keyword: only active with -fparameter-passing (implied by
// -fsafecxx).

struct X { int n; };

static void sink(in X) noexcept {}

static void take(forward X x) noexcept { sink(x); } // expected-note {{candidate function not viable}}

static void call() noexcept {
  X x{1};
  take(x);
}

// With a concrete X the provisional X& lowering cannot bind a const object;
// a deduced `forward R` can (R becomes const X), which is the form span uses.
static void concrete_limitation(const X cx) noexcept {
  take(cx); // expected-error {{no matching function for call to 'take'}}
}

// Written `&` is redundant for any borrow keyword, including forward.
static void bad(forward X &x) noexcept { // expected-error {{a forward borrow already means a reference}}
  (void)x;
}

// An identifier named `forward` is still an ordinary identifier elsewhere.
static int forward = 3;
static int use_name() noexcept { return forward; }

// `forward R r` lowers to R&: its address has type R* (const is preserved).
// The function has a raw pointer in its signature, so it is implicitly unsafe.
template <typename R>
[[unsafe]] static R *addr(forward R r) noexcept { return &r; }

static void addrs() noexcept {
  X x{};
  const X cx{};
  [[unsafe]] {
    (void)addr(x);
    (void)addr(cx);
  }
}
