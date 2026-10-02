// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm %s -o - | FileCheck %s

// Herbception `catch return_failure(expr)` with a payload that the ABI returns
// indirectly. The catch-fails object is {union{T value; E error}; bool} and
// the error member must be addressed by its own index inside the union, not
// by index 0 -- with a payload larger than the error, index 0 is the value.

struct Big { long long a, b, c, d; };
struct E2 { long long x, y; };

// CHECK: define {{.*}} { %struct.Big, i8 } @_Z5mkbigDE2E2Ei(
Big mkbig(int n) return_failure{E2} { return {n, n + 1, n + 2, n + 3}; }

// The discriminant comes from the aggregate, and the payload is read back
// out of the result's union member zero (the value), not the error member.
// CHECK-LABEL: define dso_local noundef i64 @_Z3fooi(i32 noundef %0)
// CHECK: call { %struct.Big, i8 } @_Z5mkbigDE2E2Ei
// CHECK: ret i64
long long foo(int x) {
  auto e = catch return_failure(mkbig(x));
  return !e.failed ? e.value.d : e.error.x;
}
