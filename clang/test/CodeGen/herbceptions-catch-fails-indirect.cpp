// RUN: %clang -fherbceptions -fno-exceptions -S -emit-llvm -o - %s | FileCheck %s

// Herbception `catch return_failure(expr)` with a payload that the ABI returns
// indirectly. On Win64 a payload wider than the register budget is built in
// caller storage behind a `throws_sret` pointer, so the call returns {E, i1} and
// the payload has to be read back out of that storage. The first element of the
// returned aggregate is then the error alone, not the payload: taking the
// payload from it silently yields the error and whatever follows it in memory.

struct Big { long long a, b, c, d; };
struct E2 { long long x, y; };

// CHECK: define dso_local { %struct.E2, i1 } @_Z5mkbigi(ptr noalias writable throws_sret(%struct.Big) align 8 %{{.*}}, i32
Big mkbig(int n) return_failure{E2} { return {n, n + 1, n + 2, n + 3}; }

// CHECK-LABEL: define dso_local noundef i64 @_Z3fooi(i32 noundef %0)
// CHECK:         %[[CALL:.*]] = call { %struct.E2, i1 } @_Z5mkbigi(ptr {{.*}}throws_sret(%struct.Big){{.*}} %[[SLOT:.*]], i32
// The discriminant comes from the aggregate.
// CHECK:         %[[DISC:.*]] = extractvalue { %struct.E2, i1 } %[[CALL]], 1
// CHECK:         %[[ZF:.*]] = zext i1 %[[DISC]] to i8
// The payload is read from the storage the callee was given, not from the
// aggregate's first element (which holds the error).
// CHECK:         %[[VAL:.*]] = load %struct.Big, ptr %[[SLOT]]
// CHECK:         store %struct.Big %[[VAL]]
long long foo(int x) {
  auto e = catch return_failure(mkbig(x));
  return !e.failed ? e.value.d : e.error.x;
}
