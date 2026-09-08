// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++26 -fherbceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=ITANIUM

// Regression test: a `try { } catch throws(...)` block inside a function template
// should not emit "cannot use 'try' with exceptions disabled" under
// -fherbceptions -fno-exceptions. The template instantiation path goes through
// TransformCXXTryStmt which must also check for herbception handlers.

namespace std {
struct error {
  void *domain;
  __UINTPTR_TYPE__ code;
};
}

template<typename T>
void foo(T val) try {
  // use val to avoid unused warning
  (void)val;
} catch throws(::std::error) {
  // handler
}

// ITANIUM-LABEL: define linkonce_odr void @_Z3fooIiEvT_(
void test() {
  foo<int>(42);
}
