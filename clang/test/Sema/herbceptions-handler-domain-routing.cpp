// RUN: %clang_cc1 -std=c++20 -fherbceptions -fcxx-exceptions -fexceptions \
// RUN:   -fsyntax-only -verify %s

// A CXXTryExpr is built before the containing statement's handlers are
// parsed. Domain validation must therefore follow the actual destination:
// typed propagation through a traditional-only try keeps E unchanged, while
// a catch-throws destination requires a valid E-to-std::error conversion.

namespace std {
struct error_domain_singleton {};
struct error {
  error_domain_singleton const *domain;
  __SIZE_TYPE__ code;
};
template <class T> class error_domain;
} // namespace std

struct routed_error {};
struct missing_error {};
struct legacy_error {};

namespace std {
template <> class error_domain<::routed_error> {
public:
  // This is valid C++, but deliberately not a valid error-domain accessor.
  // It must be irrelevant when the selected destination retains routed_error.
  static __SIZE_TYPE__ domain() noexcept { return 1; }
  static __SIZE_TYPE__ code(::routed_error) noexcept { return 0; }
};
} // namespace std

int routed_failure() return_failure{routed_error} {
  return_failure routed_error{};
}

int missing_failure() return_failure{missing_error} {
  return_failure missing_error{};
}

void basic_failure() throws;

int traditional_only() return_failure{routed_error} {
  try {
    // No std::error destination exists, so this raw E propagation is valid
    // despite the unrelated malformed specialization above.
    return routed_failure();
  } catch (legacy_error &) {
    return 1;
  }
}

int handler_requires_conversion() return_failure{routed_error} {
  try {
    return routed_failure(); // expected-error {{no std::error_domain specialization}}
  } catch throws(std::error error) {
    (void)error;
    return 2;
  }
}

int outer_handler_sees_through_traditional_try()
    return_failure{routed_error} {
  try {
    try {
      // The inner traditional clause does not consume deterministic failure;
      // post-handler resolution must find the outer catch-throws destination.
      return routed_failure(); // expected-error {{no std::error_domain specialization}}
    } catch (legacy_error &) {
      return 3;
    }
  } catch throws(std::error error) {
    (void)error;
    return 4;
  }
}

void discarded_domain_uses_are_not_destinations() {
  try {
    if constexpr (false) {
      (void)routed_failure();
      (void)missing_failure();
    }
  } catch throws(std::error error) {
    (void)error;
  }
}

void basic_throws_discards_missing_domain() throws {
  if constexpr (false)
    (void)missing_failure();
}

template <bool Live> void dependent_basic_throws() throws {
  if constexpr (Live)
    (void)missing_failure(); // expected-error {{no std::error_domain specialization}}
}

template void dependent_basic_throws<false>();
template void dependent_basic_throws<true>(); // expected-note {{in instantiation of function template specialization}}

void local_class_bodies_are_not_try_body_destinations() {
  try {
    struct local {
      int member = missing_failure();
      void method() {
        try {
          (void)missing_failure(); // expected-error {{call to 'throws' function}}
        } catch (legacy_error &) {
        }
      }
    };
    // Variable initialization executes in the surrounding try body and must
    // remain traversed even though the adjacent record declaration is skipped.
    int value = routed_failure(); // expected-error {{no std::error_domain specialization}}
    (void)value;
  } catch throws(std::error error) {
    (void)error;
  }
}

void nested_herb_handler_routes_outward() {
  try {
    try {
      basic_failure();
    } catch throws(std::error inner) {
      (void)inner;
      // A handler cannot catch a new failure raised by its own body. The
      // enclosing catch-throws destination therefore owns this conversion.
      (void)routed_failure(); // expected-error {{no std::error_domain specialization}}
    }
  } catch throws(std::error outer) {
    (void)outer;
  }
}

void traditional_handler_routes_to_later_herb_handler() {
  try {
    throw legacy_error{};
  } catch (legacy_error &) {
    // Ordinary call syntax must gain the same propagation node as a call in
    // the try body; post-handler resolution then sees the later sibling.
    (void)routed_failure(); // expected-error {{no std::error_domain specialization}}
  } catch throws(std::error error) {
    (void)error;
  }
}

int traditional_handler_after_herb_remains_raw()
    return_failure{routed_error} {
  try {
    throw legacy_error{};
  } catch throws(std::error error) {
    (void)error;
    return 5;
  } catch (legacy_error &) {
    // This clause has no later catch-throws sibling. Its typed failure leaves
    // through the matching function channel without consulting error_domain.
    return routed_failure();
  }
}
