// RUN: %clang_cc1 -std=c++20 -triple wasm32-unknown-wasip1 -fherbceptions -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s

// The WebAssembly C++ ABI returns 'this' from constructors and destructors
// (constructorsAndDestructorsReturnThis), so the 'this' parameter normally
// gets the 'returned' attribute. A `throws` constructor, however, returns the
// error discriminant union rather than 'this', and the IR verifier requires a
// 'returned' parameter's type to match the function's return type. The 'this'
// parameter of a `throws` constructor must therefore not carry 'returned'.

namespace std {
struct error { void *d; __SIZE_TYPE__ c; };
}

struct Foo {
  int x;
  Foo(int v) throws : x(v) {}
};

struct Bar {
  int x;
  Bar(int v) : x(v) {}
};

Foo *make_foo() throws { return new Foo(1); }
Bar *make_bar() { return new Bar(1); }

// The `throws` ctor returns {{ptr, i32}, i1}; 'this' is not returned.
// CHECK: define {{.*}} { { ptr, i32 }, i1 } @_ZN3FooC{{[12]}}Ei(ptr noundef nonnull align 4 dereferenceable(4) %
// A plain ctor still returns 'this' and keeps 'returned'.
// CHECK: define {{.*}} ptr @_ZN3BarC{{[12]}}Ei(ptr noundef nonnull returned align 4 dereferenceable(4) %
