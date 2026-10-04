// RUN: %clang_cc1 %s -std=c++2c -fsyntax-only -verify
// expected-no-diagnostics

// An iterating expansion statement inside a templated function is expanded
// when the function is instantiated. When that instantiation is triggered
// while sema is checking an *unrelated* templated declaration (here: the
// non-dependent call in 'trigger'), the parser's current scope still
// belongs to that declaration and has a template-parameter parent. That
// stray scope used to mark the internal '[&] consteval { ... }()' — which
// the compiler parses to compute the expansion size — dependent; the
// 'auto' return type of its 'operator()' then stayed undeduced, the call
// was type-dependent, and the expansion failed silently, poisoning the
// enclosing specialization for all later uses.

template <unsigned N> struct iarr {
  unsigned v[N];
  constexpr const unsigned *begin() const { return v; }
  constexpr const unsigned *end() const { return v + N; }
};
template <unsigned N> constexpr iarr<N> idxs{};

template <unsigned N>
consteval int count() {
  int r = 0;
  template for (constexpr auto i : idxs<N>)
    r += (int)i + 1;
  return r;
}

struct B {};

// 'count<2>()' is a non-dependent call inside a function template, so it is
// checked at definition time; 'count<2>' is instantiated eagerly while the
// parser's scope still belongs to 'trigger'.
template <typename T>
constexpr int trigger(B b) { return count<2>(); }

// 'count<2>' must remain usable afterwards.
static_assert(count<2>() == 2);
