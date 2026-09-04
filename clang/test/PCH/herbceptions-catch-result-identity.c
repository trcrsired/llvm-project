// RUN: %clang_cc1 -x c-header -std=c23 -fherbceptions -emit-pch \
// RUN:   -o %t.pch %S/Inputs/herbceptions-catch-result-identity-c.h
// RUN: %clang_cc1 -std=c23 -fherbceptions -include-pch %t.pch -verify %s

// C has no template-specialization identity, so a cache miss after loading a
// PCH must recover the synthetic record by its compiler-owned structural
// marker, canonical alternative types, and validated field shape.
int c_pch_make_unsugared(void) return_failure { int };
typedef __typeof__(catch return_failure(
    c_pch_make_unsugared())) fresh_c_catch_result;
_Static_assert(
    __builtin_types_compatible_p(imported_c_catch_result, fresh_c_catch_result),
    "the imported and freshly requested N2289 carriers must be one C type");

c_pch_anonymous_value c_pch_make_anonymous_fresh(void)
    return_failure { c_pch_anonymous_error };
typedef __typeof__(catch return_failure(c_pch_make_anonymous_fresh()))
    fresh_c_anonymous_catch_result;
_Static_assert(__builtin_types_compatible_p(
    imported_c_anonymous_catch_result, fresh_c_anonymous_catch_result));

typedef struct {
  int member;
} c_fresh_anonymous_value;
typedef struct {
  int member;
} c_fresh_anonymous_error;
c_fresh_anonymous_value c_make_independent_anonymous(void)
    return_failure { c_fresh_anonymous_error };
typedef __typeof__(catch return_failure(c_make_independent_anonymous()))
    independent_c_anonymous_catch_result;
_Static_assert(!__builtin_types_compatible_p(
    imported_c_anonymous_catch_result,
    independent_c_anonymous_catch_result));

// Deserializing the hidden compiler record must not make its reserved tag a
// source-language redeclaration candidate. Even an exact user-written layout
// denotes an ordinary C tag rather than the compiler-owned <T, E> carrier.
struct __herb_catch_fails {
  union {
    int value;
    int error;
  };
  _Bool failed;
};
typedef struct __herb_catch_fails c_pch_user_collision_result;
_Static_assert(!__builtin_types_compatible_p(
    imported_c_catch_result, c_pch_user_collision_result));

// expected-no-diagnostics
