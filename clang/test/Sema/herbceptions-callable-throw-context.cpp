// RUN: %clang_cc1 -std=c++26 -fherbceptions -fblocks -fcxx-exceptions \
// RUN:   -fexceptions -fsyntax-only -verify %s
//
// A try body and each handler belong to one callable's control-flow graph.
// A closure declared in that source range cannot branch to the enclosing
// callable's handler when it is invoked later, so neither explicit failure
// creation nor bare rethrow may inherit the enclosing lexical context.

namespace std {
struct error_domain_singleton {
  void (*do_cleanup)(unsigned long) noexcept = 0;
  bool (*do_equivalent)(unsigned long, error_domain_singleton const *,
                        unsigned long) noexcept = 0;
  void (*do_name)(unsigned long, int, void *, void *) noexcept = 0;
  void (*do_message)(unsigned long, int, void *, void *) noexcept = 0;
  int (*do_to_errc)(unsigned long) noexcept = 0;
};

class error {
public:
  error() = delete;
  error(error const &) = delete;
  error &operator=(error const &) = delete;
  constexpr ~error() noexcept {}

private:
  void const *domain_opaque{};
  __SIZE_TYPE__ code_opaque{};
  explicit constexpr error(void const *domain, __SIZE_TYPE__ code) noexcept
      : domain_opaque(domain), code_opaque(code) {}
};

template <typename T> struct error_domain;
enum class callable_error : int { failure = 9 };
inline error_domain_singleton callable_domain{};

template <> struct error_domain<callable_error> {
  static error_domain_singleton const *domain() noexcept {
    return &callable_domain;
  }
  static __SIZE_TYPE__ code(callable_error value) noexcept {
    return static_cast<__SIZE_TYPE__>(value);
  }
};
} // namespace std

struct legacy_error {};

int fail_callable_error() return_failure{std::callable_error} {
  return_failure std::callable_error::failure;
}

void outer_try_is_not_a_closure_handler() {
  try {
    auto lambda = [] {
      throw throws std::callable_error::failure;
      // expected-error@-1 {{'throw throws' is only allowed inside a function declared 'throws' or 'return_failure{...}'}}
    };
    auto block = ^{
      throw throws std::callable_error::failure;
      // expected-error@-1 {{'throw throws' is only allowed inside a function declared 'throws' or 'return_failure{...}'}}
    };
    (void)lambda;
    (void)block;
  } catch throws(std::error) {
  }
}

void outer_herb_handler_is_not_a_closure_handler() {
  try {
    throw throws std::callable_error::failure;
  } catch throws(std::error) {
    // The direct rethrow reads this handler's own slot.
    if (false)
      throw throws;

    auto lambda = [] {
      throw throws;
      // expected-error@-1 {{bare 'throw throws' (rethrow) is only allowed inside a 'catch throws' block}}
    };
    auto block = ^{
      throw throws;
      // expected-error@-1 {{bare 'throw throws' (rethrow) is only allowed inside a 'catch throws' block}}
    };
    auto active_lambda = []() throws {
      // A return channel permits a new error, but it cannot supply a caught
      // error slot for the operand-less rethrow form.
      throw throws;
      // expected-error@-1 {{bare 'throw throws' (rethrow) is only allowed inside a 'catch throws' block}}
    };
    auto active_fails_lambda = []() throws {
      // This callable owns its basic std::error channel. It must not inherit
      // the enclosing handler's active slot when converting a raw fails{E}
      // callee through its own auto-propagation path.
      (void)fail_callable_error();
    };
    auto active_fails_block = ^() throws {
      // Blocks are independent callable boundaries under the same rule.
      (void)fail_callable_error();
    };
    (void)lambda;
    (void)block;
    (void)active_lambda;
    (void)active_fails_lambda;
    (void)active_fails_block;
  }
}

template <bool Enabled> void transformed_handler_ownership() {
  try {
    throw throws std::callable_error::failure;
  } catch throws(std::error) {
    if constexpr (Enabled) {
      // Rebuilding this statement during instantiation must retain the
      // current handler's caught-error owner.
      if (false)
        throw throws;
    }
  }
}

template <bool Enabled> void transformed_clause_ownership() {
  try {
    throw legacy_error{};
  } catch (legacy_error &) {
    if constexpr (Enabled) {
      // A transformed traditional catch is still a catch clause of this try,
      // so deterministic propagation may target its following handler.
      if (false)
        throw throws std::callable_error::failure;
    }
  } catch throws(std::error) {
  }
}

template void transformed_handler_ownership<true>();
template void transformed_clause_ownership<true>();

void outer_traditional_handler_is_not_a_closure_handler() {
  try {
    throw legacy_error{};
  } catch (legacy_error &) {
    // A direct deterministic throw may chain to the following handler.
    if (false)
      throw throws std::callable_error::failure;

    auto lambda = [] {
      throw throws std::callable_error::failure;
      // expected-error@-1 {{'throw throws' is only allowed inside a function declared 'throws' or 'return_failure{...}'}}
    };
    auto block = ^{
      throw throws std::callable_error::failure;
      // expected-error@-1 {{'throw throws' is only allowed inside a function declared 'throws' or 'return_failure{...}'}}
    };
    (void)lambda;
    (void)block;
  } catch throws(std::error) {
  }
}

void closures_may_own_nested_handlers() {
  auto lambda = [] {
    try {
      throw throws std::callable_error::failure;
    } catch throws(std::error) {
      if (false)
        throw throws;
    }
  };
  auto block = ^{
    try {
      throw throws std::callable_error::failure;
    } catch throws(std::error) {
      if (false)
        throw throws;
    }
  };
  (void)lambda;
  (void)block;
}
