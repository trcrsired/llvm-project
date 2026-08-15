// RUN: %clang_cc1 -std=c++26 -fherbceptions -fcxx-exceptions -fexceptions -emit-llvm -o - %s | FileCheck %s

// A `try { } catch throws(std::error e)` block also catches legacy C++
// exceptions: calls to noexcept(false) functions inside the try become
// invokes to a catch-all landing pad, which fabricates a std::error
// {error_domain<std::cxa_exception_code>::domain(), thrown_object_ptr} and
// routes it to the handler.

namespace std {
struct error {
  void *d;
  unsigned long c;
  ~error() noexcept;
};
struct cxa_exception_code { unsigned long p; };
struct error_domain_singleton;
template <class T> class error_domain;
template <> class error_domain<cxa_exception_code> {
public:
  static inline constexpr error_domain_singleton const *domain() noexcept;
  static inline unsigned long code(cxa_exception_code) noexcept { return 0; }
};
}

extern void foo();

// CHECK: define dso_local void @_Z3barv() #[[ATTR:[0-9]+]] personality ptr @__gxx_personality_v0 {
// CHECK: invoke void @_Z3foov()
// CHECK:         to label %invoke.cont unwind label %lpad
// CHECK: herb.legacy.convert:
// CHECK: call {{.*}} @_ZNSt12error_domainISt18cxa_exception_codeE6domainEv()
// CHECK: call ptr @__cxa_get_exception_ptr(ptr %exn)
// CHECK: landingpad { ptr, i32 }
// CHECK:         catch ptr null
// CHECK: br label %herb.legacy.convert
// CHECK: catch.throws:
// CHECK: call void @_ZNSt5errorD1Ev(ptr {{.*}} %e)
void bar() {
  try {
    foo();
  } catch throws(std::error e) {
    (void)e;
  }
}

// CHECK: attributes #[[ATTR]] = { {{.*}} }
