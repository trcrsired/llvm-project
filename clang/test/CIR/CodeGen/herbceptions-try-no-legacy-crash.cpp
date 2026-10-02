// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm %s -o - | FileCheck %s

// try() on a throws call inside a throws function must not run the legacy
// exception-conversion path: the error already rides the shaped return.

int callee() throws;

// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z3fooDrv()
// CHECK: call { { ptr, i64 }, i8 } @_Z6calleeDrv()
// CHECK: ret { { ptr, i64 }, i8 }
int foo() throws {
  return try(callee());
}

// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z3barDrv()
// CHECK: call { { ptr, i64 }, i8 } @_Z6calleeDrv()
// CHECK: ret { { ptr, i64 }, i8 }
int bar() throws {
  return try(callee());
}
