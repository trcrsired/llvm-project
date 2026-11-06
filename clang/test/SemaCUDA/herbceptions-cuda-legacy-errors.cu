// RUN: %clang_cc1 -triple nvptx64-nvidia-cuda -fcuda-is-device -std=c++26 -fcxx-exceptions -fsyntax-only -verify -x cuda %s
// RUN: %clang_cc1 -triple amdgcn-amd-amdhsa -fcuda-is-device -std=c++26 -fcxx-exceptions -fsyntax-only -verify -x cuda %s
// RUN: %clang_cc1 -triple nvptx64-nvidia-cuda -fcuda-is-device -std=c++26 -fsyntax-only -verify=noeh -x cuda %s

// Legacy C++ exceptions should still be diagnosed on CUDA device code.
// This ensures we haven't broken the existing diagnostic for non-herbceptions
// code. The CUDA-specific diagnostic is the one DiagnoseExceptionUse reports
// when C++ exceptions are enabled -- with -fno-exceptions it deliberately falls
// through to the generic 'exceptions disabled' diagnostic, so both modes are
// checked here.

// expected-error@+2 {{cannot use 'try' in __device__ function}}
// noeh-error@+1 {{cannot use 'try' with exceptions disabled}}
__attribute__((device)) void kernel() try {
  // GPU kernel body
} catch(...) {
  // legacy handler
}
