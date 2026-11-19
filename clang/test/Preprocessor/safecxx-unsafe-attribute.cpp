// RUN: %clang -fsafecxx -E -P -x c++ %s | FileCheck %s --check-prefix=ON
// RUN: %clang -fsafecxx-unsafe -E -P -x c++ %s | FileCheck %s --check-prefix=ON
// RUN: %clang -E -P -x c++ %s | FileCheck %s --check-prefix=OFF

// __has_cpp_attribute(unsafe) is non-zero only when the fence is enabled,
// either through the umbrella or through the fence-only toggle. Other
// compilers always report 0 and ignore the attribute.

#if __has_cpp_attribute(unsafe)
FENCE_ON
#else
FENCE_OFF
#endif

// ON: FENCE_ON
// ON-NOT: FENCE_OFF
// OFF: FENCE_OFF
// OFF-NOT: FENCE_ON
