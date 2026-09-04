// RUN: %clang_cc1 -std=c++20 -fherbceptions -fsyntax-only -verify %s

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
};
struct error_domain_singleton {};
struct typed_error {
  __SIZE_TYPE__ value;
};
template <class T> class error_domain;
template <> class error_domain<typed_error> {
public:
  static error_domain_singleton const *domain() noexcept;
  static __SIZE_TYPE__ code(typed_error) noexcept;
};
}

struct conversion {
  operator int() throws;
};

// An implicit user-defined conversion is a real call. Its deterministic
// failure channel therefore propagates exactly like explicit call syntax.
int via_conversion(conversion value) throws {
  return value;
}

struct operand {};
int operator+(operand, operand) throws;

// Overloaded operator notation is likewise an invocation even though it does
// not pass through ActOnCallExpr, where explicit calls acquire CXXTryExpr.
int via_operator(operand lhs, operand rhs) throws {
  return lhs + rhs;
}

int operator-(operand, operand) return_failure{std::typed_error};

// A typed free operator must use the same CXXTryExpr conversion as an explicit
// call when it propagates into the implicit std::error channel.
int via_typed_operator(operand lhs, operand rhs) throws {
  return lhs - rhs;
}

int operator~(operand) return_failure{std::typed_error};

// Unary notation has the same typed conversion requirements as binary
// notation and must not depend on its distinct overload-resolution entry point.
int via_typed_unary(operand value) throws {
  return ~value;
}

// Operator notation must obey the same destination rule as f(args). Neither
// overload-resolution entry point may leave an unchecked failure carrier in a
// plain function merely because it bypasses ActOnCallExpr.
int reject_unhandled_binary(operand lhs, operand rhs) {
  return lhs + rhs; // expected-error {{call to 'throws' function in a non-'throws' function}}
}

int reject_unhandled_unary(operand value) {
  return ~value; // expected-error {{call to 'throws' function in a non-'throws' function}}
}

int reject_nested_lambda(operand lhs, operand rhs) throws {
  auto nested = [=] {
    return lhs + rhs; // expected-error {{call to 'throws' function in a non-'throws' function}}
  };
  return 0;
}

struct callable {
  int operator()() throws;
};

// Function-call operator syntax does pass through ActOnCallExpr and remains a
// control case against double wrapping or double evaluation.
int via_call_operator(callable fn) throws {
  return fn();
}

int via_statement(conversion value) throws {
  int result = value;
  return result;
}
