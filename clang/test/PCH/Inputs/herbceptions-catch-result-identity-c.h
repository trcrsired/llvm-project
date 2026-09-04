#ifndef HERBCEPTIONS_C_CATCH_RESULT_PCH_H
#define HERBCEPTIONS_C_CATCH_RESULT_PCH_H

typedef int c_payload_alias;
c_payload_alias c_pch_make_value(void) return_failure { int };

typedef __typeof__(catch return_failure(
    c_pch_make_value())) imported_c_catch_result;

typedef struct {
  int member;
} c_pch_anonymous_value;
typedef struct {
  int member;
} c_pch_anonymous_error;
c_pch_anonymous_value c_pch_make_anonymous(void)
    return_failure { c_pch_anonymous_error };
typedef __typeof__(catch return_failure(c_pch_make_anonymous()))
    imported_c_anonymous_catch_result;

#endif
