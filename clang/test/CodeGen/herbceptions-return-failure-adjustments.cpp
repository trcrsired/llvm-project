// REQUIRES: native
// RUN: %clangxx -std=c++20 -fherbceptions %s -o %t
// RUN: %t

// Exercise both standard designator adjustments through the ABI carrier.  The
// returned addresses must be the original array and function, not materialized
// temporaries or values produced by a broader conversion sequence.

char array_payload[] = "array payload";

int function_payload() { return 42; }

char *array_result(bool fail) return_failure{char *} {
  if (fail)
    return_failure array_payload;
  return nullptr;
}

using function_pointer = int (*)();

function_pointer function_result(bool fail)
    return_failure{function_pointer} {
  if (fail)
    return_failure function_payload;
  return nullptr;
}

int main() {
  auto array_failure = catch return_failure(array_result(true));
  if (!array_failure.failed || array_failure.error != array_payload)
    return 1;

  auto function_failure = catch return_failure(function_result(true));
  if (!function_failure.failed || function_failure.error != function_payload ||
      function_failure.error() != 42)
    return 2;

  auto array_success = catch return_failure(array_result(false));
  if (array_success.failed || array_success.value != nullptr)
    return 3;

  auto function_success = catch return_failure(function_result(false));
  if (function_success.failed || function_success.value != nullptr)
    return 4;

  return 0;
}
