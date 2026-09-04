#pragma once
#include "herbceptions-catch-result-common.h"

using module_catch_result_a = decltype(catch return_failure(
    module_result_source(static_cast<int *>(nullptr), false)));

inline int use_module_catch_result_a(int *destructions) {
  auto result = catch return_failure(module_result_source(destructions, false));
  return result.failed ? -1 : 1;
}
