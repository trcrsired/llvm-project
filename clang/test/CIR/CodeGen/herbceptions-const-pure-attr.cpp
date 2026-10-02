// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm %s -o - 2>&1 | FileCheck %s

// 'const'/'pure' are honored on herbceptions functions: the failure
// discriminant is part of the return value ({union{T,E}, i8}), so call
// elision/dedup preserves it and the attributes remain sound.

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
  // CHECK: call { { ptr, i64 }, i8 } @_Z3cofDri(i32 noundef %{{.*}}) #[[CONST_CALL:[0-9]+]]
  return try(cof(x));
}

int caller2(int x) throws {
  // CHECK: call { { ptr, i64 }, i8 } @_Z3pufDri(i32 noundef %{{.*}}) #[[PURE_CALL:[0-9]+]]
  return try(puf(x));
}

int caller3(int x) throws {
  // CHECK: call { i32, i8 } @_Z4coffDEiEi(i32 noundef %{{.*}}) #[[CONST_CALL]]
  return try(coff(x));
}

Big bcaller(int x) throws {
  // CIR keeps the wire form for throws calls: the payload-sized record is
  // returned directly instead of going through a throws_sret pointer.
  // CHECK: call { %struct.Big, i8 } @_Z4cobfDri(i32 noundef %{{.*}}) #[[CONST_CALL]]
  return try(cobf(x));
}

// CHECK: attributes #[[CONST_CALL]] = { {{.*}}memory(none){{.*}} }
// CHECK: attributes #[[PURE_CALL]] = { {{.*}}memory(read{{.*}}) }
