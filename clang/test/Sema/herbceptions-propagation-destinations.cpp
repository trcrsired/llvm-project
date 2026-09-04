// RUN: %clang_cc1 -std=c++20 -fherbceptions -fcxx-exceptions -fexceptions \
// RUN:   -fsyntax-only -verify %s

// Propagation compatibility is selected from the completed control-flow
// graph, not from the lexical handler being parsed. Typed destinations require
// the same canonical E; only a basic std::error destination invokes the domain
// conversion.

namespace std {
struct error_domain_singleton {};
struct error {
  void *domain;
  __SIZE_TYPE__ code;
};
template <class T> struct error_domain;
} // namespace std

struct error_a { int value; };
struct error_b { int value; };
struct legacy_error {};

namespace std {
inline error_domain_singleton domain_a;
template <> struct error_domain<::error_a> {
  static error_domain_singleton *domain() noexcept { return &domain_a; }
  static int code(::error_a value) noexcept { return value.value; }
};
} // namespace std

int fail_a() return_failure{error_a};
int fail_b() return_failure{error_b};
int fail_basic() throws;

int typed_same() return_failure{error_a} { return fail_a(); }

int typed_mismatch() return_failure{error_b} {
  return fail_a(); // expected-error {{failure types must match exactly}}
}

int basic_to_typed() return_failure{error_a} {
  return fail_basic(); // expected-error {{handle it with 'catch throws'}}
}

int typed_to_basic() throws { return fail_a(); }

// Top-level cv is not part of the payload identity, whereas pointee cv is.
using const_error_a = const error_a;
int fail_const_a() return_failure{const_error_a};
int top_level_cv_matches() return_failure{error_a} { return fail_const_a(); }

int fail_pointer() return_failure{int *};
int pointer_cv_mismatch() return_failure{const int *} {
  return fail_pointer(); // expected-error {{failure types must match exactly}}
}

int handler_body_routes_to_function() return_failure{error_a} {
  try {
    return fail_basic();
  } catch throws(std::error) {
    // This handler's slot is no longer active while its body runs.
    return fail_a();
  }
}

int handler_body_mismatch() return_failure{error_b} {
  try {
    return fail_basic();
  } catch throws(std::error) {
    return fail_a(); // expected-error {{failure types must match exactly}}
  }
}

int handler_body_basic_to_typed() return_failure{error_a} {
  try {
    return fail_basic();
  } catch throws(std::error) {
    return fail_basic(); // expected-error {{handle it with 'catch throws'}}
  }
}

void handler_body_routes_to_outer_handler() {
  try {
    try {
      (void)fail_basic();
    } catch throws(std::error) {
      (void)fail_a();
    }
  } catch throws(std::error) {
  }
}

void traditional_handler_routes_to_later_handler() {
  try {
    throw legacy_error{};
  } catch (legacy_error &) {
    (void)fail_a();
  } catch throws(std::error) {
  }
}

int discarded_mismatch() return_failure{error_b} {
  if constexpr (false)
    return fail_a();
  return 0;
}

template <bool Live> int dependent_mismatch() return_failure{error_b} {
  if constexpr (Live)
    return fail_a(); // expected-error {{failure types must match exactly}}
  return 0;
}

template int dependent_mismatch<false>();
template int dependent_mismatch<true>(); // expected-note {{in instantiation of function template specialization}}

struct cxx_std_error {
  void const *domain;
  __SIZE_TYPE__ code;
};
int fail_c_bridge() return_failure{cxx_std_error};
int strict_c_bridge_to_basic() throws { return fail_c_bridge(); }

namespace nested {
struct cxx_std_error {
  void *domain;
  __SIZE_TYPE__ code;
};
int fail_nested_bridge() return_failure{cxx_std_error};
} // namespace nested

int reject_non_global_bridge() throws {
  return nested::fail_nested_bridge(); // expected-error {{no std::error_domain specialization}}
}
