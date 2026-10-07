// CIR counterpart of
// clang/test/CodeGen/herbceptions-legacy-convert-dllimport.cpp, Itanium
// only: CIR does not implement the Windows ABIs yet, so only the non-Windows
// (no dllimport) behavior is ported.
//
// The compiler-fabricated legacy-exception-to-herbception conversion calls
// __cxa_error_domain_*_exception_ptr and __cxa_error_code_*_exception_ptr;
// on non-Windows targets they carry no dllimport.

// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++26 -fherbceptions -fcxx-exceptions -fexceptions -fclangir -emit-llvm -o - %s | FileCheck %s

namespace std {
struct error {
  void *d;
  __SIZE_TYPE__ c;
  ~error() noexcept;
};
struct exception_ptr;
}

extern void foo();

// On Linux (non-Windows), no dllimport on the declarations.
// CHECK: declare ptr @__cxa_error_domain_itanium_exception_ptr()
// CHECK: declare i64 @__cxa_error_code_itanium_exception_ptr(i64 noundef, ptr noundef, ptr noundef, ptr noundef)
void qux() {
  try {
    foo();
  } catch throws(std::error e) {
    (void)e;
  }
}
