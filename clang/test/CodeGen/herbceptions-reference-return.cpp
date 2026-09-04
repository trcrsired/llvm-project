// RUN: %clang_cc1 -std=c++20 -fherbceptions -emit-llvm -o - %s | FileCheck %s

// A reference result occupies the success payload as a pointer.  The error
// union may be wider than that pointer, but propagation must reload only the
// successful pointer and must never allocate or copy a `value` temporary.

struct value {
  int member;
};

value global{41};

// CHECK-LABEL: define{{.*}} { { ptr, i64 }, i1 } @_Z13source_lvaluev()
// CHECK: store ptr @global, ptr
// CHECK: ret { { ptr, i64 }, i1 }
value &source_lvalue() throws { return global; }

// CHECK-LABEL: define{{.*}} { { ptr, i64 }, i1 } @_Z13source_xvaluev()
// CHECK: store ptr @global, ptr
// CHECK: ret { { ptr, i64 }, i1 }
value &&source_xvalue() throws { return static_cast<value &&>(global); }

// CHECK-LABEL: define{{.*}} { { ptr, i64 }, i1 } @_Z16automatic_lvaluev()
// CHECK: call { { ptr, i64 }, i1 } @_Z13source_lvaluev()
// CHECK: extractvalue { { ptr, i64 }, i1 } {{.*}}, 0
// CHECK: load ptr, ptr
// CHECK-NOT: load i32
// CHECK: ret { { ptr, i64 }, i1 }
decltype(auto) automatic_lvalue() throws { return source_lvalue(); }

// CHECK-LABEL: define{{.*}} { { ptr, i64 }, i1 } @_Z16automatic_xvaluev()
// CHECK: call { { ptr, i64 }, i1 } @_Z13source_xvaluev()
// CHECK: load ptr, ptr
// CHECK-NOT: load i32
// CHECK: ret { { ptr, i64 }, i1 }
decltype(auto) automatic_xvalue() throws { return source_xvalue(); }

int evaluations;

value &observed(bool fail) return_failure{int} {
  ++evaluations;
  if (fail)
    return_failure 23;
  return global;
}

// CHECK-LABEL: define{{.*}} { ptr, i1 } @_Z17explicit_observedb(
// CHECK-COUNT-1: call { ptr, i1 } @_Z8observedb(
// CHECK: extractvalue { ptr, i1 } {{.*}}, 1
// CHECK: br i1
// CHECK: load ptr, ptr
// CHECK-NOT: call { ptr, i1 } @_Z8observedb(
// CHECK: ret { ptr, i1 }
value &explicit_observed(bool fail) return_failure{int} {
  return try(observed(fail));
}

// CHECK-LABEL: define{{.*}} { ptr, i1 } @_Z18automatic_observedb(
// CHECK-COUNT-1: call { ptr, i1 } @_Z8observedb(
// CHECK: extractvalue { ptr, i1 } {{.*}}, 1
// CHECK: br i1
// CHECK: load ptr, ptr
// CHECK-NOT: call { ptr, i1 } @_Z8observedb(
// CHECK: ret { ptr, i1 }
value &automatic_observed(bool fail) return_failure{int} {
  return observed(fail);
}
