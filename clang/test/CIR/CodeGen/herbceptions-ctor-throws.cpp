// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm -o - %s | FileCheck %s

// CIR counterpart of clang/test/CodeGen/herbceptions-ctor-throws.cpp: a
// `throws` constructor whose member-initializer calls a `throws` function.
// The constructor must be lowered with the {E, i8} ABI like any other throws
// function, giving its body a valid payload slot to store the propagated
// error.

namespace std {
struct error { void *d; __SIZE_TYPE__ c; };
}

int open_resource() throws;

struct A {
  int fd;
  A() throws : fd(open_resource()) {}
};

// A `throws` constructor invoked from a `throws` function propagates the
// error. The call site must agree with the constructor's {E, i1} definition
// ABI, and (since Sema does not wrap constructor calls in `try(expr)`) the
// auto-propagate path stores the payload and sets the discriminant.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z4makeDrv(
// CHECK:         call { { ptr, i64 }, i8 } @_ZN1AC2EDrv(
int make() throws {
  A a{};
  return a.fd;
}

// The constructor itself carries the {E, i1} return ABI (E = std::error, the
// implicit 2-register {void*, size_t} struct), so its member-initializer call
// to a throws function can store the error payload and set the discriminant
// on failure instead of crashing.
// CHECK: define {{.*}} { { ptr, i64 }, i8 } @_ZN1AC{{[12]}}EDrv(
