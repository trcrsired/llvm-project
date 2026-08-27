// RUN: not %clang_cc1 -fherbceptions -fsyntax-only %s 2>&1 | FileCheck %s

int bar(int x) fails{int} { return x; }

// In C, calling a fails{E} function without try() or catch fails() is a
// compile-time error.
// CHECK: error: calling function with 'fails{...}' specifier requires 'try()' or 'catch fails()' wrapper
// CHECK-NOT: error: expected function body
int foo(int x) {
  return bar(x);
}
