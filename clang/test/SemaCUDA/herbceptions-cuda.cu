// RUN: %clang_cc1 -triple nvptx64-nvidia-cuda -fcuda-is-device -std=c++26 -fherbceptions -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple amdgcn-amd-amdhsa -fcuda-is-device -std=c++26 -fherbceptions -fsyntax-only -verify %s
// expected-no-diagnostics

// Herbceptions should be allowed on GPU targets (CUDA/AMDGPU) even with
// -fno-exceptions, since herbceptions provide their own error propagation
// mechanism that doesn't rely on legacy C++ EH infrastructure.

namespace std {
struct error {
  void *domain;
  __UINTPTR_TYPE__ code;
};
}

void kernel() try {
  // GPU kernel body
} catch throws(::std::error) {
  // herbception handler - should be allowed on GPU
}

// Also test in a template (exercises TransformCXXTryStmt path)
template<typename T>
void gpu_template(T val) try {
  (void)val;
} catch throws(::std::error) {
  // handler
}

void test() {
  gpu_template<int>(42);
}
