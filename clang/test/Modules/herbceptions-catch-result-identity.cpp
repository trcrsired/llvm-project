// RUN: rm -rf %t && mkdir -p %t
// RUN: %clangxx -std=c++20 -fherbceptions -fno-exceptions -fmodules \
// RUN:   -fmodule-map-file=%S/Inputs/herbceptions-catch-result.modulemap \
// RUN:   -fmodules-cache-path=%t -I%S/Inputs %s -o %t/test
// RUN: %t/test
// REQUIRES: native

#include "herbceptions-catch-result-a.h"
#include "herbceptions-catch-result-b.h"

using fresh_module_catch_result = decltype(catch return_failure(
    module_result_source(static_cast<int *>(nullptr), false)));
static_assert(__is_same(module_catch_result_a, module_catch_result_b));
static_assert(__is_same(module_catch_result_a, fresh_module_catch_result));

int main() {
  int destructions = 0;
  if (use_module_catch_result_a(&destructions) != 1 || destructions != 1)
    return 1;
  if (use_module_catch_result_b(&destructions) != 1 || destructions != 1)
    return 2;
  return 0;
}
