// RUN: %clang -fherbceptions -std=c++26 -O2 -S -emit-llvm -o - %s | FileCheck %s

// std::error::release() in the trivial catch-and-release case: after the
// rebind to a cleanup-free domain, the [[assume(do_cleanup == nullptr)]]
// inside release() lets the optimizer delete the std::error's destructor
// entirely — and the domain fetch as well, since nothing observes the
// rebound error. The payload rides the {domain, code} channel pair
// end-to-end.

using size_t = __SIZE_TYPE__;

namespace std {
struct error_domain_singleton {
  void (*do_cleanup)(unsigned long) noexcept;
};

struct error {
  void const *domain;
  size_t code;
  [[nodiscard]] inline __cxx_std_error release() noexcept;
  constexpr ~error() noexcept {
    [[assume(domain != nullptr)]];
    auto dc{static_cast<error_domain_singleton const *>(domain)->do_cleanup};
    if (dc) dc(code);
  }
};

// Opaque (like __cxa_error_domain_posix()): the optimizer cannot see what
// it returns, so only the assume inside release() makes the dtor fold.
extern "C" error_domain_singleton const *__test_posix_domain();

inline __cxx_std_error error::release() noexcept {
  __cxx_std_error e{domain, code};
  domain = __test_posix_domain();
  code = 0;
  [[assume(static_cast<error_domain_singleton const *>(domain)->do_cleanup ==
           nullptr)]];
  return e;
}
} // namespace std

int f() throws;

// CHECK-LABEL: define dso_local noundef i32 @_Z1gv(
// CHECK: call { { ptr, i64 }, i1 } @_Z1fDrv()
// CHECK: extractvalue { { ptr, i64 }, i1 } %{{.*}}, 1
// CHECK: br i1 %{{.*}}, label %{{.*}}, label %{{.*}}
// CHECK: extractvalue { ptr, i64 } %{{.*}}, 1
// CHECK-NOT: call {{.*}}errorD1Ev
// CHECK-NOT: call {{.*}}__test_posix_domain
// CHECK: ret i32
int g() {
  try {
    f();
    return 0;
  } catch throws(std::error e) {
    auto rel = e.release();
    return static_cast<int>(rel.code);
  }
}
