// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm %s -o - | FileCheck %s

// Regression test: a `try { } catch throws(...)` block inside a function
// template should not emit "cannot use 'try' with exceptions disabled" under
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
  (void)val;
} catch throws(::std::error) {
}

// CHECK-LABEL: define dso_local void @_Z4testv()
// CHECK: call void @_Z3fooIiEvT_(i32 noundef 42)
void test() {
  foo<int>(42);
}

// CHECK-LABEL: define linkonce_odr void @_Z3fooIiEvT_(
