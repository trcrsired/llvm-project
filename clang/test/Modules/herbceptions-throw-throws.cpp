// RUN: rm -rf %t && mkdir -p %t
// RUN: split-file %s %t
//
// RUN: %clang_cc1 -std=c++26 -fherbceptions -triple x86_64-unknown-linux-gnu -emit-module-interface -o %t/m.pcm %t/m.cppm
// RUN: %clang_cc1 -std=c++26 -fherbceptions -triple x86_64-unknown-linux-gnu -fmodule-file=m=%t/m.pcm -emit-llvm -disable-llvm-passes -o - %t/use.cpp | FileCheck %s
//
// A 'throw throws' statement inside a module-defined function must keep its
// herbception kind through AST serialization. When the importer re-emits the
// deserialized body, the error value must be returned through the {payload, i1}
// throws channel; it must NOT be lowered to a legacy __cxa_throw of the
// fabricated std::error object. (Regression test: the IsHerbception bit of
// CXXThrowExpr was not serialized.)

//--- std_stub.h
#pragma once
namespace std {
struct error_domain_singleton {};
struct error {
  void *d;
  __SIZE_TYPE__ c;
};
struct my_errc { int v; };
template <class T> class error_domain;
extern error_domain_singleton herb_dom;
template <> class error_domain<my_errc> {
public:
  static inline constexpr error_domain_singleton const *domain() noexcept {
    return &herb_dom;
  }
  static inline __SIZE_TYPE__ code(my_errc e) noexcept {
    return static_cast<__SIZE_TYPE__>(e.v);
  }
};
} // namespace std
inline constexpr void *operator new(__SIZE_TYPE__, void *p) noexcept {
  return p;
}

//--- m.cppm
module;
#include "std_stub.h"
export module m;

export namespace std {
using ::std::error;
using ::std::my_errc;
}

export struct Widget {
  inline Widget(int x) throws { throw throws std::my_errc{x}; }
};

// Mirrors a container's emplace: a module template whose throws-spec is
// dependent, placement-newing a module-defined type with a throws ctor.
export template <typename T, typename... Args>
inline T *construct_in(void *p, Args &&...args)
    throws(!noexcept(T(static_cast<Args &&>(args)...))) {
  return ::new (p) T(static_cast<Args &&>(args)...);
}

//--- use.cpp
import m;

int f(void *p) {
  try {
    (void)construct_in<Widget, int>(p, 0);
  } catch throws(std::error e) {
    return static_cast<int>(e.c);
  }
  return 0;
}

// The dependent-throws template instantiation keeps the channel.
// CHECK-LABEL: define {{.*}}linkonce_odr { { ptr, i64 }, i1 } @_ZW1m12construct_inIS_6WidgetJiEEDgntnxcvT_spscT0_fL4294967294p0_EPS2_PvDpOS3_(
// CHECK-NOT: __cxa_throw
// CHECK-NOT: __cxa_allocate_exception
// CHECK: ret { { ptr, i64 }, i1 }

// The re-emitted ctor body must raise the error through the throws channel and
// return { { ptr, i64 }, i1 } with the error flag -- never __cxa_throw.
// CHECK-LABEL: define {{.*}}linkonce_odr { { ptr, i64 }, i1 } @_ZNW1m6WidgetC2EDri(
// CHECK-NOT: __cxa_throw
// CHECK-NOT: __cxa_allocate_exception
// CHECK: ret { { ptr, i64 }, i1 }
