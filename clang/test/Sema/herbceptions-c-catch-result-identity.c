// RUN: %clang_cc1 -std=c23 -fherbceptions -fsyntax-only -verify %s

struct value_a {
  int member;
};
struct value_b {
  int member;
};

struct value_a make_a(void) return_failure{int};
struct value_b make_b(void) return_failure{int};

typedef __typeof__(catch return_failure(make_a())) result_a_first;
typedef __typeof__(catch return_failure(make_a())) result_a_second;
typedef __typeof__(catch return_failure(make_b())) result_b;

// Repeated requests for one canonical <T, E> pair denote the same carrier,
// while structurally identical alternatives do not erase pair identity.
_Static_assert(__builtin_types_compatible_p(result_a_first, result_a_second));
_Static_assert(!__builtin_types_compatible_p(result_a_first, result_b));

// A source declaration with the reserved spelling and exact physical layout
// is still a user tag, not a compiler-owned catch-result family member.
struct __herb_catch_fails {
  union {
    struct value_a value;
    int error;
  };
  _Bool failed;
};
_Static_assert(!__builtin_types_compatible_p(
    result_a_first, struct __herb_catch_fails));

// expected-no-diagnostics
