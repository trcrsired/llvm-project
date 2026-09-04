#ifndef HERBCEPTIONS_C_MODULE_CATCH_RESULT_B_H
#define HERBCEPTIONS_C_MODULE_CATCH_RESULT_B_H

#include "herbceptions-c-catch-result-common.h"

typedef __typeof__(catch return_failure(c_module_value_source()))
    c_module_same_result_b;
typedef __typeof__(catch return_failure(c_module_other_source()))
    c_module_distinct_result_b;

typedef struct {
  int member;
} c_module_private_value_b;
typedef struct {
  int member;
} c_module_private_error_b;
c_module_private_value_b c_module_private_source_b(void)
    return_failure{c_module_private_error_b};
typedef __typeof__(catch return_failure(c_module_private_source_b()))
    c_module_private_result_b_first;
typedef __typeof__(catch return_failure(c_module_private_source_b()))
    c_module_private_result_b_second;

#endif
