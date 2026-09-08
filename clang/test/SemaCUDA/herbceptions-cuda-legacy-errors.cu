// RUN: %clang_cc1 -triple nvptx64-nvidia-cuda -fcuda-is-device -std=c++26 -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple amdgcn-amd-amdhsa -fcuda-is-device -std=c++26 -fsyntax-only -verify %s

// Legacy C++ exceptions should still be diagnosed on CUDA device code.
// This ensures we haven't broken the existing diagnostic for non-herbceptions
// code.

void kernel() try { // expected-error {{cannot use 'try' in __device__ function}}
  // GPU kernel body
} catch(...) {
  // legacy handler
}
