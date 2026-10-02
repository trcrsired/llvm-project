// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm %s -o - | FileCheck %s

// A `throws` constructor called through `new` must propagate its shaped
// {payload, i8} result so the caller's `return new Foo(1)` auto-propagates.

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

// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z8make_fooDrv()
// CHECK: call { { ptr, i64 }, i8 } @_ZN3FooC1EDri(ptr {{.*}}, i32 noundef 1)
// CHECK: ret { { ptr, i64 }, i8 }
Foo *make_foo() throws { return new Foo(1); }

// A non-throws constructor returns `this` conventionally through a void call.
// CHECK-LABEL: define dso_local noundef ptr @_Z8make_barv()
// CHECK: call void @_ZN3BarC1Ei(ptr {{.*}}, i32 noundef 1)
// CHECK: ret ptr
Bar *make_bar() { return new Bar(1); }
