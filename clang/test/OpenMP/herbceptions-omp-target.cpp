// RUN: %clang_cc1 -fopenmp -triple nvptx64 -fopenmp-is-target-device -std=c++26 -fherbceptions %s -verify -Wopenmp-target-exception -analyze
// RUN: %clang_cc1 -fopenmp -triple amdgcn-amd-amdhsa -fopenmp-is-target-device -std=c++26 -fherbceptions %s -verify -Wopenmp-target-exception -analyze

// Herbceptions should be allowed on OpenMP GPU targets even with
// -fno-exceptions, since herbceptions provide their own error propagation
// mechanism that doesn't rely on legacy C++ EH infrastructure.

namespace std {
struct error {
  void *domain;
  __UINTPTR_TYPE__ code;
};
}

#pragma omp declare target
void foo() try {
  // GPU target body
} catch throws(::std::error) {
  // herbception handler - should be allowed on GPU
}
#pragma omp end declare target
