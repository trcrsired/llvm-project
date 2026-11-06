// RUN: %clang -std=c++26 -fherbceptions -S -emit-llvm -o - %s | FileCheck %s

// A herbception failure return is not an unwind: it stores the error in the
// return channel and returns. The destructors of the subobjects built so far,
// and the operator delete of a new-expression whose initializer failed, are
// exception-only (EH-active, normal-inactive) cleanups, so nothing else would
// run them. Each case below must run them on the failure edge.

namespace std {
struct error_domain_singleton {};
struct error {
  void *d;
  __SIZE_TYPE__ c;
};
struct my_errc { int v; };
template <class T> class error_domain;
template <> class error_domain<my_errc> {
public:
  static inline constexpr error_domain_singleton const *domain() noexcept;
  static inline __SIZE_TYPE__ code(my_errc) noexcept { return 0; }
};
}

struct part {
  part() noexcept;
  ~part() noexcept;
};
struct boom {
  boom() throws;
};
struct bbase {
  bbase() noexcept;
  ~bbase() noexcept;
};
struct loc {
  loc() noexcept;
  ~loc() noexcept;
};

// A failure in the second mem-initializer destroys the first member.
//
// CHECK-LABEL: define {{.*}}@_ZN9meminit_tC2EDrv(
// CHECK:         call void @_ZN4partC1Ev(
// CHECK:         call {{.*}}@_ZN4boomC1EDrv(
// CHECK:         br i1
// CHECK:         call void @_ZN4partD{{[12]}}Ev(
struct meminit_t {
  part a;
  boom b;
  meminit_t() throws;
};
meminit_t::meminit_t() throws : a{}, b{} {}

// A `throw throws` in the constructor body destroys the body's locals and then
// the members, in reverse order of construction. The local's destructor runs
// through the normal cleanup threading, the members' through the failure edge,
// so the local comes first.
//
// CHECK-LABEL: define {{.*}}@_ZN6body_tC2EDrv(
// CHECK:         call void @_ZN3locC1Ev(
// CHECK:         call void @_ZN3locD{{[12]}}Ev(
// CHECK:         call void @_ZN4partD{{[12]}}Ev(
// CHECK:         call void @_ZN4partD{{[12]}}Ev(
// CHECK:         ret {{.*}}i1
struct body_t {
  part a;
  part b;
  body_t() throws;
};
body_t::body_t() throws : a{}, b{} {
  loc l;
  throw throws std::my_errc{1};
}

// A base class subobject is destroyed the same way.
//
// CHECK-LABEL: define {{.*}}@_ZN7derivedC2EDrv(
// CHECK:         call void @_ZN5bbaseD{{[12]}}Ev(
struct derived : bbase {
  derived() throws;
};
derived::derived() throws { throw throws std::my_errc{2}; }

// A `catch throws` handler in the constructor body keeps the members alive, so
// that failure edge abandons nothing.
//
// CHECK-LABEL: define {{.*}}@_ZN6kept_tC2EDrv(
// CHECK-NOT:     call void @_ZN4partD{{[12]}}Ev
// CHECK:         ret {{.*}}i1
struct kept_t {
  part a;
  kept_t() throws;
};
kept_t::kept_t() throws : a{} {
  try {
    throw throws std::my_errc{3};
  } catch throws (std::error e) {
  }
}

// The storage of a new-expression whose initializer fails is freed.
//
// CHECK-LABEL: define {{.*}}@_Z5mknewDrv(
// CHECK:         {{invoke|call}} {{.*}}@_Znwm(
// CHECK:         call void @_ZdlPv{{m?}}(
// CHECK:         ret {{.*}}i1
meminit_t *mknew() throws { return new meminit_t; }
