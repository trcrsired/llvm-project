#ifndef HERBCEPTIONS_C_MODULE_CATCH_RESULT_COMMON_H
#define HERBCEPTIONS_C_MODULE_CATCH_RESULT_COMMON_H

struct c_module_value {
  int member;
};

struct c_module_other_value {
  int member;
};

struct c_module_value c_module_value_source(void) return_failure{int};
struct c_module_other_value c_module_other_source(void) return_failure{int};

#endif
