// RUN: %clang_cc1 -std=c++26 -fherbceptions -fsyntax-only -verify %s

// A `return_failure{E}` failure may only reach a channel whose payload type
// differs from E -- the implicit std::error channel of a `throws` function or
// of a `catch throws` handler, or a mismatched `return_failure{E2}` channel --
// through std::error_domain<E>. Without a visible specialization the raw E
// payload would be bit-copied into the channel slot (a miscompile), so it is
// diagnosed the same way `throw throws E` is. This matches GCC.
//
// Regression test for the clang herbceptions bugs:
//   bugs/llvm_herbceptions/rf-propagation-through-throws
//   bugs/llvm_herbceptions/catch-throws-on-rf-call

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
};
struct error_domain_singleton {};
enum class ec : unsigned { bad = 7 };
template <class T> class error_domain;
template <> class error_domain<ec> {
public:
  static error_domain_singleton *domain() noexcept;
  static __SIZE_TYPE__ code(ec) noexcept;
};
} // namespace std

int f() return_failure{int};
int fe() return_failure{std::ec};

// Auto-propagating a `return_failure{int}` error into the implicit std::error
// channel of a `throws` function needs std::error_domain<int>.
int auto_in_throws() throws { return f(); } // expected-error {{'throw throws' operand type 'int' has no std::error_domain specialization}}
int explicit_try_in_throws() throws {
  return try(f()); // expected-error {{'throw throws' operand type 'int' has no std::error_domain specialization}}
}

// A `catch throws(std::error)` handler binds the std::error channel, so a
// `return_failure{int}` call routed to it needs the conversion too.
int catch_throws_handler() {
  try {
    return f(); // expected-error {{'throw throws' operand type 'int' has no std::error_domain specialization}}
  } catch throws(std::error e) {
    return (int)e.code;
  }
}

// A mismatched `return_failure{long}` channel likewise needs a conversion.
long mismatch_return_failure() return_failure{long} {
  return f(); // expected-error {{'throw throws' operand type 'int' has no std::error_domain specialization}}
}

// A matching `return_failure` channel needs no domain.
int auto_in_return_failure() return_failure{int} { return f(); }
int explicit_try_in_return_failure() return_failure{int} { return try(f()); }

// With a visible std::error_domain specialization the conversion is emitted.
int auto_in_throws_with_domain() throws { return fe(); }
int catch_throws_handler_with_domain() {
  try {
    return fe();
  } catch throws(std::error e) {
    return (int)e.code;
  }
}
