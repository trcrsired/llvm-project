// RUN: rm -rf %t && mkdir -p %t/forward %t/reverse
// RUN: %clang_cc1 -std=c23 -fherbceptions -fmodules -fimplicit-module-maps \
// RUN:   -fmodule-map-file=%S/Inputs/herbceptions-c-catch-result.modulemap \
// RUN:   -fmodules-cache-path=%t/forward -I%S/Inputs -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c23 -fherbceptions -fmodules -fimplicit-module-maps \
// RUN:   -fmodule-map-file=%S/Inputs/herbceptions-c-catch-result.modulemap \
// RUN:   -fmodules-cache-path=%t/reverse -I%S/Inputs \
// RUN:   -DREVERSE_IMPORT_ORDER -fsyntax-only -verify %s

#ifdef REVERSE_IMPORT_ORDER
#include "herbceptions-c-catch-result-user.h"
#include "herbceptions-c-catch-result-b.h"
#include "herbceptions-c-catch-result-a.h"
#else
#include "herbceptions-c-catch-result-a.h"
#include "herbceptions-c-catch-result-user.h"
#include "herbceptions-c-catch-result-b.h"
#endif

typedef __typeof__(catch return_failure(c_module_value_source()))
    c_module_same_result_fresh;
typedef __typeof__(catch return_failure(c_module_other_source()))
    c_module_distinct_result_fresh;

// Independent modules requesting the same canonical <T, E> pair denote one
// compiler-owned C tag, and a new request in the importing source reuses it.
_Static_assert(__builtin_types_compatible_p(c_module_same_result_a,
                                            c_module_same_result_b));
_Static_assert(__builtin_types_compatible_p(c_module_same_result_a,
                                            c_module_same_result_fresh));
_Static_assert(__builtin_types_compatible_p(c_module_distinct_result_a,
                                            c_module_distinct_result_b));
_Static_assert(__builtin_types_compatible_p(c_module_distinct_result_a,
                                            c_module_distinct_result_fresh));

// The shared reserved tag spelling is not its structural identity: different
// canonical alternatives must remain incompatible after module merging.
_Static_assert(!__builtin_types_compatible_p(c_module_same_result_a,
                                             c_module_distinct_result_a));

// Repeated requests for anonymous alternatives within one module reuse one
// carrier. Equally shaped anonymous alternatives owned by independent modules
// are different canonical types and therefore must not merge their carriers.
_Static_assert(__builtin_types_compatible_p(
    c_module_private_result_a_first, c_module_private_result_a_second));
_Static_assert(__builtin_types_compatible_p(
    c_module_private_result_b_first, c_module_private_result_b_second));
_Static_assert(!__builtin_types_compatible_p(
    c_module_private_result_a_first, c_module_private_result_b_first));

// A user record with the reserved spelling and exact carrier field layout is
// still a source-language tag, not the compiler-owned <T, E> specialization.
_Static_assert(!__builtin_types_compatible_p(c_module_same_result_a,
                                             c_module_user_collision_result));

// expected-no-diagnostics
