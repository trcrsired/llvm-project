// RUN: %clang_cc1 -std=c++14 -fherbceptions \
// RUN:   -fsyntax-only -verify %s
// expected-no-diagnostics

int plain_function();
int bare_function() throws;
int true_function() throws(true);
int false_function() throws(false);
int noexcept_function() noexcept;

// Pre-C++17 rejects exception specifications written directly in type aliases.
// Derive the function-pointer types from declarations so this test exercises
// canonical ABI identity without relying on a later language grammar rule.
static_assert(__is_same(decltype(&bare_function), decltype(&true_function)),
              "bare throws and throws(true) share one ABI");
static_assert(!__is_same(decltype(&bare_function), decltype(&plain_function)),
              "an active channel remains ABI-significant before C++17");
static_assert(__is_same(decltype(&false_function),
                        decltype(&noexcept_function)),
              "throws(false) is the ordinary non-throwing state");

using error_alias = int;
int typed_alias_function() return_failure{error_alias};
int typed_canonical_function() return_failure{int};
static_assert(__is_same(decltype(&typed_alias_function),
                        decltype(&typed_canonical_function)),
              "typed payload aliases canonicalize before C++17");
