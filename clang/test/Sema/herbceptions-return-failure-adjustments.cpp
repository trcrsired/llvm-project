// RUN: %clang_cc1 -std=c++20 -fherbceptions -fsyntax-only -verify %s

// The failure operand denotes the object stored in the typed carrier.  C++
// array-to-pointer and function-to-pointer adjustments establish that object
// type without authorizing a different implicit or user-defined conversion.

char array_payload[] = "array payload";

int function_payload() { return 42; }

int accepts_array_designator() return_failure{char *} {
  return_failure array_payload;
}

using function_pointer = int (*)();

function_pointer accepts_function_designator()
    return_failure{function_pointer} {
  return_failure function_payload;
}

const char const_array_payload[] = "const array payload";

int rejects_pointee_qualification() return_failure{char *} {
  // The array adjustment yields const char *, not the declared char * error
  // alternative; qualification conversion is deliberately not performed.
  // expected-error@+1 {{'return_failure' operand}}
  return_failure const_array_payload;
}

long rejects_numeric_conversion() return_failure{long} {
  return_failure 1; // expected-error {{'return_failure' operand}}
}

struct base_error {};
struct derived_error : base_error {};

derived_error derived_payload;

base_error *rejects_derived_pointer_conversion()
    return_failure{base_error *} {
  // A derived-to-base pointer conversion changes the declared active object
  // type just as surely as a scalar conversion; exact carrier identity must
  // therefore reject it even though an ordinary initialization would accept.
  // expected-error@+1 {{'return_failure' operand}}
  return_failure &derived_payload;
}

struct target_error {};

struct convertible_error {
  operator target_error() const;
};

target_error rejects_user_defined_conversion() return_failure{target_error} {
  // expected-error@+1 {{'return_failure' operand}}
  return_failure convertible_error{};
}
