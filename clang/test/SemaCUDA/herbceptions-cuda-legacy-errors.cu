// RUN: %clang_cc1 -triple nvptx64-nvidia-cuda -fcuda-is-device -std=c++26 -fno-exceptions -fsyntax-only -verify %s

// Legacy C++ exceptions should still be diagnosed on CUDA device code.
// This ensures we haven't broken the existing diagnostic for non-herbceptions
// code.

void kernel() try { // expected-error {{cannot use 'try' with exceptions disabled}}
  // GPU kernel body
} catch(...) { // expected-note {{}}
  // legacy handler
}
