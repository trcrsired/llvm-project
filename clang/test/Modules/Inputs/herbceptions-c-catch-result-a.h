#ifndef HERBCEPTIONS_C_MODULE_CATCH_RESULT_A_H
#define HERBCEPTIONS_C_MODULE_CATCH_RESULT_A_H

#include "herbceptions-c-catch-result-common.h"

typedef __typeof__(catch return_failure(c_module_value_source()))
    c_module_same_result_a;
typedef __typeof__(catch return_failure(c_module_other_source()))
    c_module_distinct_result_a;

typedef struct {
  int member;
} c_module_private_value_a;
typedef struct {
  int member;
} c_module_private_error_a;
c_module_private_value_a c_module_private_source_a(void)
    return_failure{c_module_private_error_a};
typedef __typeof__(catch return_failure(c_module_private_source_a()))
    c_module_private_result_a_first;
typedef __typeof__(catch return_failure(c_module_private_source_a()))
    c_module_private_result_a_second;

#endif
