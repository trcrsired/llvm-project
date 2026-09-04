#ifndef HERBCEPTIONS_C_MODULE_CATCH_RESULT_USER_H
#define HERBCEPTIONS_C_MODULE_CATCH_RESULT_USER_H

#include "herbceptions-c-catch-result-common.h"

// A source-written record may collide with the reserved implementation
// spelling, but it must never acquire compiler-owned carrier identity.
struct __herb_catch_fails {
  union {
    struct c_module_value value;
    int error;
  };
  _Bool failed;
};
typedef struct __herb_catch_fails c_module_user_collision_result;

#endif
