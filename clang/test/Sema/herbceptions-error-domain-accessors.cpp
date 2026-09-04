// RUN: %clang_cc1 -std=c++20 -fherbceptions \
// RUN:   -fsyntax-only -verify %s

// A visible error_domain specialization is a real C++ customization point.
// Malformed accessors must be rejected by Sema with normal call semantics;
// none may reach CodeGen's ABI lowering and trigger an assertion.

namespace std {
struct error_domain_singleton {};
struct error {
  error_domain_singleton const *domain;
  __SIZE_TYPE__ code;
};
template <class T> class error_domain;
inline error_domain_singleton singleton;
} // namespace std

#define DEFINE_FAILURE(Name)                                                     \
  struct Name {};                                                               \
  int fail_##Name() return_failure{Name} { return_failure Name{}; }

DEFINE_FAILURE(code_zero)
namespace std {
template <> class error_domain<::code_zero> {
public:
  static error_domain_singleton const *domain() noexcept { return &singleton; }
  static __SIZE_TYPE__ code() noexcept { return 0; } // expected-note {{'code' declared here}}
};
} // namespace std
void reject_code_zero() {
  try {
    (void)fail_code_zero(); // expected-error {{too many arguments}}
  } catch throws(std::error) {
  }
}

DEFINE_FAILURE(code_two)
namespace std {
template <> class error_domain<::code_two> {
public:
  static error_domain_singleton const *domain() noexcept { return &singleton; }
  static __SIZE_TYPE__ code(::code_two, ::code_two) noexcept { return 0; } // expected-note {{'code' declared here}}
};
} // namespace std
void reject_code_two() {
  try {
    (void)fail_code_two(); // expected-error {{too few arguments}}
  } catch throws(std::error) {
  }
}

DEFINE_FAILURE(domain_one)
namespace std {
template <> class error_domain<::domain_one> {
public:
  static error_domain_singleton const *domain(::domain_one) noexcept { // expected-note {{'domain' declared here}}
    return &singleton;
  }
  static __SIZE_TYPE__ code(::domain_one) noexcept { return 0; }
};
} // namespace std
void reject_domain_one() {
  try {
    (void)fail_domain_one(); // expected-error {{too few arguments}}
  } catch throws(std::error) {
  }
}

DEFINE_FAILURE(domain_integer)
namespace std {
template <> class error_domain<::domain_integer> {
public:
  static __SIZE_TYPE__ domain() noexcept { return 1; }
  static __SIZE_TYPE__ code(::domain_integer) noexcept { return 0; }
};
} // namespace std
void reject_integer_domain() {
  try {
    (void)fail_domain_integer(); // expected-error {{no std::error_domain specialization}}
  } catch throws(std::error) {
  }
}

DEFINE_FAILURE(domain_function_pointer)
namespace std {
template <> class error_domain<::domain_function_pointer> {
  static error_domain_singleton const *function_domain() noexcept {
    return &singleton;
  }

public:
  static auto domain() noexcept -> decltype(&function_domain) {
    return &function_domain;
  }
  static __SIZE_TYPE__ code(::domain_function_pointer) noexcept { return 0; }
};
} // namespace std
void reject_function_pointer_domain() {
  try {
    (void)fail_domain_function_pointer(); // expected-error {{no std::error_domain specialization}}
  } catch throws(std::error) {
  }
}

DEFINE_FAILURE(code_pointer)
namespace std {
template <> class error_domain<::code_pointer> {
public:
  static error_domain_singleton const *domain() noexcept { return &singleton; }
  static void *code(::code_pointer) noexcept { return nullptr; }
};
} // namespace std
void reject_pointer_code() {
  try {
    (void)fail_code_pointer(); // expected-error {{no std::error_domain specialization}}
  } catch throws(std::error) {
  }
}

DEFINE_FAILURE(reference_code)
namespace std {
inline __SIZE_TYPE__ reference_code_value;
template <> class error_domain<::reference_code> {
public:
  static error_domain_singleton const *domain() noexcept { return &singleton; }
  // The expression's referred-to type is integral, but the accessor ABI
  // returns an address. The conversion contract requires an integer value.
  static __SIZE_TYPE__ &code(::reference_code) noexcept {
    return reference_code_value;
  }
};
} // namespace std
void reject_reference_code() {
  try {
    (void)fail_reference_code(); // expected-error {{no std::error_domain specialization}}
  } catch throws(std::error) {
  }
}

DEFINE_FAILURE(deleted_code)
namespace std {
template <> class error_domain<::deleted_code> {
public:
  static error_domain_singleton const *domain() noexcept { return &singleton; }
  static __SIZE_TYPE__ code(::deleted_code) noexcept = delete; // expected-note {{explicitly marked deleted here}}
};
} // namespace std
void reject_deleted_code() {
  try {
    (void)fail_deleted_code(); // expected-error {{attempt to use a deleted function}}
  } catch throws(std::error) {
  }
}

DEFINE_FAILURE(private_code)
namespace std {
template <> class error_domain<::private_code> {
  static error_domain_singleton const *domain() noexcept { return &singleton; } // expected-note {{implicitly declared private here}}
public:
  static __SIZE_TYPE__ code(::private_code) noexcept { return 0; }
};
} // namespace std
void reject_private_code() {
  try {
    (void)fail_private_code(); // expected-error {{'domain' is a private member}}
  } catch throws(std::error) {
  }
}

DEFINE_FAILURE(throwing_code)
namespace std {
template <> class error_domain<::throwing_code> {
public:
  static error_domain_singleton const *domain() noexcept { return &singleton; }
  static __SIZE_TYPE__ code(::throwing_code) throws { return 0; }
};
} // namespace std
void reject_throwing_code() {
  try {
    (void)fail_throwing_code(); // expected-error {{no std::error_domain specialization}}
  } catch throws(std::error) {
  }
}

DEFINE_FAILURE(legacy_throwing_domain)
namespace std {
template <> class error_domain<::legacy_throwing_domain> {
public:
  static error_domain_singleton const *domain() noexcept(false) {
    return &singleton;
  }
  static __SIZE_TYPE__ code(::legacy_throwing_domain) noexcept { return 0; }
};
} // namespace std
void reject_legacy_throwing_domain() {
  try {
    (void)fail_legacy_throwing_domain(); // expected-error {{no std::error_domain specialization}}
  } catch throws(std::error) {
  }
}

DEFINE_FAILURE(legacy_throwing_code)
namespace std {
template <> class error_domain<::legacy_throwing_code> {
public:
  static error_domain_singleton const *domain() noexcept { return &singleton; }
  static __SIZE_TYPE__ code(::legacy_throwing_code) noexcept(false) { return 0; }
};
} // namespace std
void reject_legacy_throwing_code() {
  try {
    (void)fail_legacy_throwing_code(); // expected-error {{no std::error_domain specialization}}
  } catch throws(std::error) {
  }
}

DEFINE_FAILURE(consteval_domain)
namespace std {
template <> class error_domain<::consteval_domain> {
public:
  static consteval error_domain_singleton const *domain() noexcept {
    return &singleton;
  }
  static __SIZE_TYPE__ code(::consteval_domain) noexcept { return 0; }
};
} // namespace std
void reject_consteval_domain() {
  try {
    (void)fail_consteval_domain(); // expected-error {{no std::error_domain specialization}}
  } catch throws(std::error) {
  }
}

DEFINE_FAILURE(consteval_code)
namespace std {
template <> class error_domain<::consteval_code> {
public:
  static error_domain_singleton const *domain() noexcept { return &singleton; }
  static consteval __SIZE_TYPE__ code(::consteval_code) noexcept { return 0; }
};
} // namespace std
void reject_consteval_code() {
  try {
    (void)fail_consteval_code(); // expected-error {{no std::error_domain specialization}}
  } catch throws(std::error) {
  }
}

DEFINE_FAILURE(defaulted_domain_parameter)
namespace std {
template <> class error_domain<::defaulted_domain_parameter> {
public:
  // A default argument makes the call viable but does not satisfy the exact
  // zero-parameter accessor ABI required by the std::error conversion.
  static error_domain_singleton const *domain(int = 0) noexcept {
    return &singleton;
  }
  static __SIZE_TYPE__ code(::defaulted_domain_parameter) noexcept { return 0; }
};
} // namespace std
void reject_defaulted_domain_parameter() {
  try {
    (void)fail_defaulted_domain_parameter(); // expected-error {{no std::error_domain specialization}}
  } catch throws(std::error) {
  }
}

DEFINE_FAILURE(variadic_domain)
namespace std {
template <> class error_domain<::variadic_domain> {
public:
  static error_domain_singleton const *domain(...) noexcept {
    return &singleton;
  }
  static __SIZE_TYPE__ code(::variadic_domain) noexcept { return 0; }
};
} // namespace std
void reject_variadic_domain() {
  try {
    (void)fail_variadic_domain(); // expected-error {{no std::error_domain specialization}}
  } catch throws(std::error) {
  }
}

DEFINE_FAILURE(variadic_code)
namespace std {
template <> class error_domain<::variadic_code> {
public:
  static error_domain_singleton const *domain() noexcept { return &singleton; }
  static __SIZE_TYPE__ code(::variadic_code, ...) noexcept { return 0; }
};
} // namespace std
void reject_variadic_code() {
  try {
    (void)fail_variadic_code(); // expected-error {{no std::error_domain specialization}}
  } catch throws(std::error) {
  }
}

#undef DEFINE_FAILURE
