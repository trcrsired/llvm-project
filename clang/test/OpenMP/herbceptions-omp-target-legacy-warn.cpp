// RUN: %clang_cc1 -fopenmp -triple nvptx64 -fopenmp-is-target-device -std=c++26 %s -verify -Wopenmp-target-exception -analyze

// Without herbceptions, OpenMP target should still emit the warning about
// target not supporting exception handling. This ensures we haven't broken
// the existing behavior.

#pragma omp declare target
void foo() try { // warn-warning {{target 'nvptx64' does not support exception handling; 'catch' block is ignored}}
  // GPU target body
} catch(...) {
  // legacy handler
}
#pragma omp end declare target
