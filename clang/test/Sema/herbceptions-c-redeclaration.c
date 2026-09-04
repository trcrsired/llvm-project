// RUN: %clang_cc1 -std=c17 -fherbceptions -Wno-deprecated-non-prototype \
// RUN:   -fsyntax-only -verify %s

typedef const int hidden_const_int;
typedef volatile int hidden_volatile_int;

// A by-value failure payload has one canonical identity after top-level cv is
// removed, including when an alias hides that qualification.
int compatible_direct_cv(const int value) return_failure{int};
int compatible_direct_cv(int value) return_failure{const int};

int compatible_hidden_cv(int values[3]) return_failure{hidden_const_int};
int compatible_hidden_cv(int *values) return_failure{hidden_volatile_int};

enum compatible_number {
  compatible_negative = -1,
  compatible_positive = 1,
};

// An enum with this range is compatible with int in Clang's C ABI, but the
// two parameter types are not canonically identical. This pair therefore
// forces ASTContext to build a composite prototype; the follow-up try proves
// that rebuilding the ordinary C parameter type retained the typed channel.
int compatible_rebuilt_parameter(enum compatible_number value)
    return_failure{hidden_const_int};
int compatible_rebuilt_parameter(int value) return_failure{int};

// Using a declaration after a compatible merge proves that the typed channel
// was retained instead of being reduced to an ordinary C function type.
int propagate_compatible(int *values) return_failure{int} {
  return try(compatible_hidden_cv(values));
}

int propagate_rebuilt_parameter(void) return_failure{int} {
  return try(compatible_rebuilt_parameter(compatible_negative));
}

int different_payload(void) return_failure{int};
// expected-note@-1 {{previous declaration is here}}
int different_payload(void) return_failure{long};
// expected-error@-1 {{conflicting types for 'different_payload'}}

int different_pointee_cv(void) return_failure{int *};
// expected-note@-1 {{previous declaration is here}}
int different_pointee_cv(void) return_failure{const int *};
// expected-error@-1 {{conflicting types for 'different_pointee_cv'}}

int typed_then_plain(void) return_failure{int};
// expected-note@-1 {{previous declaration is here}}
int typed_then_plain(void);
// expected-error@-1 {{conflicting types for 'typed_then_plain'}}

int plain_then_typed(void);
// expected-note@-1 {{previous declaration is here}}
int plain_then_typed(void) return_failure{int};
// expected-error@-1 {{conflicting types for 'plain_then_typed'}}

// In C17 an empty parameter list is a FunctionNoProtoType and cannot carry a
// typed result channel. It therefore cannot merge with a typed prototype even
// when the ordinary C default-promotion rules would accept the parameters.
int prototype_then_no_prototype(int value) return_failure{int};
// expected-note@-1 {{previous declaration is here}}
int prototype_then_no_prototype();
// expected-error@-1 {{conflicting types for 'prototype_then_no_prototype'}}

int no_prototype_then_prototype();
// expected-note@-1 {{previous declaration is here}}
int no_prototype_then_prototype(int value) return_failure{int};
// expected-error@-1 {{conflicting types for 'no_prototype_then_prototype'}}
