// RUN: %clang -fherbceptions -std=c++26 -S -fdiscard-value-names -emit-llvm -o - %s | FileCheck %s
// RUN: %clang -fherbceptions -std=c++26 -O2 -S -emit-llvm -o - %s | FileCheck %s --check-prefix=OPT

// try { throw throws cxx_std_error } catch throws(std::error): a
// __cxx_std_error operand is the channel value itself — it is copied
// verbatim into the fabricated std::error, never re-minted through an
// error_domain, and the std::error destructor (which runs the domain's
// do_cleanup) executes exactly once when the catch variable dies.

using size_t = __SIZE_TYPE__;

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
  ~error() noexcept;
};
}

// CHECK-LABEL: define dso_local void @_Z8relay_if15__cxx_std_error(
// CHECK: icmp ne ptr %{{.*}}, null
// CHECK: call void @llvm.memcpy{{.*}}(ptr align 8 %{{.*}}, ptr align 8 %{{.*}}, i64 16
// CHECK-NOT: __cxa_error_domain
// CHECK: call void @_ZNSt5errorD1Ev(
// CHECK: ret void
// OPT-LABEL: define dso_local void @_Z8relay_if15__cxx_std_error(
// OPT: br i1 %{{.*}}, label
// OPT-NOT: __cxa_error_domain
// OPT: call void @_ZNSt5errorD1Ev(
// OPT: ret void
void relay_if(__cxx_std_error err) {
  try {
    if (err.domain != nullptr) // null domain must never hit the channel
      throw throws err;
  } catch throws(std::error e) {
    (void)e.code;
  }
}
