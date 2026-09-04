#pragma once
#include "herbceptions-catch-result-common.h"

using module_catch_result_b = decltype(catch return_failure(
    module_result_source(static_cast<int *>(nullptr), false)));

inline int use_module_catch_result_b(int *destructions) {
  auto result = catch return_failure(module_result_source(destructions, true));
  return result.failed && result.error == 29 ? 1 : -1;
}
