// RUN: %clang -fherbceptions -S -fdiscard-value-names -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm -discard-value-names -o - %s | FileCheck %s --check-prefix=CIR

// Herbception `catch throws(E e) { ... }` block handler: a bare call to a
// throws function inside the try block returns {T, i1}, and on failure the
// error value is routed to the handler (which binds the exception variable)
// instead of being silently dropped in a noexcept function.

using size_t = __SIZE_TYPE__;

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
  // The error value is a compiler-fabricated {domain, code} pair whose
  // destructor (which runs the domain's do_cleanup) must execute exactly once
  // when the catch variable goes out of scope, like std::expected.
  ~error() noexcept;
};
}

// CHECK: define dso_local void @_Z6calleem(i64 noundef %0) #[[ATTR:[0-9]+]] {
// CHECK: call { { ptr, i64 }, i1 } @_Z3barDrm
// CHECK: extractvalue { { ptr, i64 }, i1 } %{{.*}}, 1
// CHECK: br i1 %{{.*}}, label %{{.*}}, label %{{.*}}
// CHECK: store {{.*}} %{{.*}}, ptr %{{.*}}, align 8
// CHECK: call void @_ZNSt5errorD1Ev(ptr {{.*}} %{{.*}})
// CIR-LABEL: define dso_local void @_Z6calleem(i64 noundef %0)
// CIR: {{call|invoke}} { { ptr, i64 }, i8 } @_Z3barDrm
// CIR: br i1 %{{.*}}, label %{{.*}}, label %{{.*}}
// CIR: call void @_ZNSt5errorD1Ev(ptr {{.*}} %{{.*}})
void bar(size_t i) throws {
  if (i == 0) throw throws std::error{nullptr, 4};
}

void callee(size_t i) noexcept {
  try {
    bar(i);
  } catch throws(std::error e) {
    (void)e.code;
  }
}

// CHECK: attributes #[[ATTR]] = { {{.*}} }
