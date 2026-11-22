// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx -verify %s

// Unevaluated operands are compile-time probes, not uses of an opaque pointer.
// The dereference check ignores them, just like the subscript, arrow and call
// checks already do, so a concept can ask whether `*t` is well-formed.

struct P {
  int *p;
};

template <class T> concept Derefable = requires(T t) { *t; };
template <class T> concept Subscriptable = requires(T t, int i) { t[i]; };
static_assert(Derefable<int *>);
static_assert(Subscriptable<int *>);

static void probes(P p) noexcept {
  (void)sizeof(*p.p);
  (void)noexcept(*p.p);
  using D = decltype(*p.p);
  (void)sizeof(D);
}

// Evaluated dereferences are still rejected.
static void evaluated(P p) noexcept {
  (void)*p.p;   // expected-error {{dereference of a pointer is not allowed}}
  (void)p.p[0]; // expected-error {{dereference of a pointer is not allowed}}
}
