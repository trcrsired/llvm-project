// RUN: %clang -std=c++20 -fherbceptions -fno-exceptions -S -fdiscard-value-names -emit-llvm -Werror -o - %s | FileCheck %s

// 'const'/'pure' are honored on herbceptions functions: the failure
// discriminant is part of the return value ({union{T,E}, i1}), so call
// elision/dedup preserves it and the attributes remain sound. -Werror also
// pins that no "attribute ignored" diagnostic is emitted.

namespace std {
struct error_domain_singleton {};
struct error {
  void *d;
  __SIZE_TYPE__ c;
};
} // namespace std

struct Big { long long a, b, c, d, e, f, g, h; }; // ABI-indirect payload

__attribute__((const)) int cof(int x) throws;
__attribute__((pure)) int puf(int x) throws;
__attribute__((const)) int coff(int x) return_failure{int};
__attribute__((const)) Big cobf(int x) throws;

int caller(int x) throws {
  // CHECK: call { { ptr, i64 }, i1 } @_Z3cofi(i32 noundef %{{.*}}) #[[CONST_CALL:[0-9]+]]
  return try(cof(x));
}
// Register return: full memory(none); the discriminant is just another
// result component.
// CHECK-LABEL: declare { { ptr, i64 }, i1 } @_Z3cofi(i32 noundef)
// CHECK-SAME:  #[[CONST_ATTRS:[0-9]+]]

int caller2(int x) throws {
  // CHECK: call { { ptr, i64 }, i1 } @_Z3pufi(i32 noundef %{{.*}}) #[[PURE_CALL:[0-9]+]]
  return try(puf(x));
}
// CHECK-LABEL: declare { { ptr, i64 }, i1 } @_Z3pufi(i32 noundef)
// CHECK-SAME:  #[[PURE_ATTRS:[0-9]+]]

int caller3(int x) throws {
  // CHECK: call { i32, i1 } @_Z4coffi(i32 noundef %{{.*}}) #[[CONST_CALL]]
  return try(coff(x));
}
// CHECK-LABEL: declare { i32, i1 } @_Z4coffi(i32 noundef)
// CHECK-SAME:  #[[CONST_ATTRS]]

Big bcaller(int x) throws {
  // CHECK: call { { ptr, i64 }, i1 } @_Z4cobfi(ptr writable throws_sret(%struct.Big) align 8 %{{.*}}, i32 noundef %{{.*}}) #[[SRET_CALL:[0-9]+]]
  return try(cobf(x));
}
// Indirect payload: the payload is written through the 'throws_sret' pointer,
// so the memory effect is weakened to argmem -- exactly like a plain 'sret'
// return. The discriminant still rides in the {E, i1} result.
// CHECK-LABEL: declare { { ptr, i64 }, i1 } @_Z4cobfi(ptr writable throws_sret(%struct.Big) align 8, i32 noundef)
// CHECK-SAME:  #[[SRET_ATTRS:[0-9]+]]

// CHECK-DAG: attributes #[[CONST_ATTRS]] = { {{.*}}throws{{.*}}willreturn memory(none){{.*}} }
// CHECK-DAG: attributes #[[PURE_ATTRS]] = { {{.*}}throws{{.*}}willreturn memory(read){{.*}} }
// CHECK-DAG: attributes #[[SRET_ATTRS]] = { {{.*}}throws{{.*}}willreturn memory(argmem: readwrite){{.*}} }
// CHECK-DAG: attributes #[[CONST_CALL]] = { {{.*}}throws{{.*}}memory(none) }
// CHECK-DAG: attributes #[[PURE_CALL]] = { {{.*}}throws{{.*}}memory(read) }
// CHECK-DAG: attributes #[[SRET_CALL]] = { {{.*}}throws{{.*}}memory(argmem: readwrite) }
