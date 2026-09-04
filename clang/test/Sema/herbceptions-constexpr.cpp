// RUN: %clang_cc1 -std=c++26 -fherbceptions -fsyntax-only -verify %s

// Constexpr `return_failure{E}` with `catch return_failure(expr)` (N2289 aggregate) and
// `try(expr)` auto-propagation.

constexpr int f(int x) return_failure{int} {
  if (x == 0) return_failure 42;
  return 2 * x;
}

constexpr int g(int x) return_failure{int} {
  return try(f(x));
}

constexpr int use_catch(int x) {
  auto r = catch return_failure(f(x));
  if (r.failed) return r.error;
  return r.value;
}

constexpr int use_catch_g(int x) {
  auto r = catch return_failure(g(x));
  if (r.failed) return r.error;
  return r.value;
}

static_assert(use_catch(3) == 6, "success");
static_assert(use_catch(0) == 42, "failure");
static_assert(use_catch_g(3) == 6, "try auto-propagate success");
static_assert(use_catch_g(0) == 42, "try auto-propagate failure");

struct reference_value {
  int member;
};

constexpr reference_value reference_object{17};

constexpr reference_value const &reference_source_lvalue()
    return_failure{int} {
  return reference_object;
}

constexpr reference_value const &&reference_source_xvalue()
    return_failure{int} {
  return static_cast<reference_value const &&>(reference_object);
}

constexpr bool reference_identity_lvalue() return_failure{int} {
  return &(try(reference_source_lvalue())) == &reference_object;
}

constexpr bool reference_identity_xvalue() return_failure{int} {
  auto &&result = try(reference_source_xvalue());
  return &result == &reference_object;
}

static_assert(reference_identity_lvalue(),
              "constexpr lvalue propagation preserves identity");
static_assert(reference_identity_xvalue(),
              "constexpr xvalue propagation preserves identity");

// `catch fails` must not be applied to a `throws` function: its error type is
// the implicit compiler-fabricated std::error, which is only handled by a
// `try { } catch throws(std::error e)` block handler.
int t(int x) throws;

int bad() {
  auto r = catch return_failure(t(0)); // expected-error {{'catch return_failure' cannot be applied to a 'throws' function; use a 'try { } catch throws(std::error e) { }' block handler instead}}
  return 0;
}
