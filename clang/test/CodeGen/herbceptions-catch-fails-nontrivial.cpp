// RUN: %clang -fherbceptions -fno-exceptions -std=c++26 -S -emit-llvm -o - %s | FileCheck %s

// A `catch return_failure(expr)` whose success type has a non-trivial
// destructor must be accepted.
//
// Clang used to store the success/error members of the N2289 aggregate in an
// anonymous union.  A C++ union with a non-trivial member has no implicit
// destructor, so the enclosing `__herb_catch_fails` got an implicitly-deleted
// destructor and binding the result failed with "attempt to use a deleted
// function" ("destructor of '__herb_catch_fails' is implicitly deleted because
// variant field 'value' has a non-trivial destructor").  That made `catch
// return_failure` unusable for every RAII type (files, buffers, strings, ...).
// The members are now stored side by side, matching GCC.
//
// The call also comes back wrapped in CXXBindTemporaryExpr (a non-trivial
// return type owns a resource), so the callee lookup must skip those wrappers;
// otherwise the error type is taken to be the success type and `r.error` has
// the wrong type.

struct T {
  int v;
  ~T() {}
};
struct E {
  int e;
};

// The members are value/error/failed, in that order.  (Like any record with a
// non-trivial member, the IR type is packed with explicit tail padding.)
// A void success type still yields the three-member aggregate; GCC gives the
// `value` member the error type in that case, and so do we (a real `void`
// field would make the layout degenerate).
// CHECK: %struct.__herb_catch_fails = type {{.*}}{ %struct.T, %struct.E, i8
// CHECK: %struct.__herb_catch_fails{{.*}} = type {{.*}}%struct.E, %struct.E, i8

// CHECK-LABEL: define {{.*}} @_Z1fDE1EEi
T f(int n) return_failure{E} { return {n}; }

// `r.error` must be E, not the success type T.
// CHECK-LABEL: define {{.*}} @_Z1gi
int g(int n) {
  auto r = catch return_failure(f(n));
  return !r.failed ? r.value.v : r.error.e;
}

// A void success type still yields the three-member aggregate; GCC gives the
// `value` member the error type in that case, and so do we (a real `void`
// field would make the layout degenerate).
// CHECK-LABEL: define {{.*}} @_Z1vDE1EEv
void v() return_failure{E} { return_failure E{1}; }

// CHECK-LABEL: define {{.*}} @_Z1hv
int h() {
  auto r = catch return_failure(v());
  return r.failed ? r.error.e : 0;
}
