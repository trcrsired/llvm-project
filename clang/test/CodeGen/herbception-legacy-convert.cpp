// RUN: %clang_cc1 -std=c++26 -fherbceptions -fcxx-exceptions -fexceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=ITANIUM
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++26 -fherbceptions -fcxx-exceptions -fexceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=MSVC
// RUN: %clang_cc1 -triple wasm32-unknown-unknown -std=c++26 -fherbceptions -fcxx-exceptions -fexceptions -exception-model=wasm -mllvm -wasm-enable-eh -emit-llvm -o - %s | FileCheck %s --check-prefix=WASM
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++26 -fherbceptions -fcxx-exceptions -fexceptions -exception-model=sjlj -emit-llvm -o - %s | FileCheck %s --check-prefix=SJLJ

// A `try { } catch throws(std::error e)` block also catches legacy C++
// exceptions: calls to noexcept(false) functions inside the try become
// invokes to a catch-all landing pad, which fabricates a std::error
// {error_domain<std::cxa_exception_code>::domain(), thrown_object_ptr} and
// routes it to the handler.

namespace std {
struct error {
  void *d;
  unsigned long long c;
  ~error() noexcept;
};
struct cxa_exception_code { unsigned long long p; };
struct error_domain_singleton;
template <class T> class error_domain;
template <> class error_domain<cxa_exception_code> {
public:
  static inline constexpr error_domain_singleton const *domain() noexcept;
  static inline unsigned long long code(cxa_exception_code) noexcept {
    return 0;
  }
};
}

extern void foo();

// ITANIUM: define dso_local void @_Z3barv() #[[ATTR:[0-9]+]] personality ptr @__gxx_personality_v0 {
// ITANIUM: invoke void @_Z3foov()
// ITANIUM:         to label %invoke.cont unwind label %lpad
// ITANIUM: herb.legacy.convert:
// ITANIUM: call {{.*}} @_ZNSt12error_domainISt18cxa_exception_codeE6domainEv()
// ITANIUM: call ptr @__cxa_get_exception_ptr(ptr %exn)
// ITANIUM: landingpad { ptr, i32 }
// ITANIUM:         catch ptr null
// ITANIUM: br label %herb.legacy.convert
// ITANIUM: catch.throws:
// ITANIUM: call void @_ZNSt5errorD1Ev(ptr {{.*}} %e)
void bar() {
  try {
    foo();
  } catch throws(std::error e) {
    (void)e;
  }
}

// MSVC uses funclet EH: catchswitch/catchpad and llvm.eh.exceptionpointer.
// MSVC: define dso_local void @"?bar@@YAXXZ"() #[[ATTR:[0-9]+]] personality ptr @__CxxFrameHandler3 {
// MSVC: invoke void @"?foo@@YAXXZ"()
// MSVC:         to label %invoke.cont unwind label %catch.dispatch
// MSVC: catchswitch within none [label %herb.legacy.convert]
// MSVC: herb.legacy.convert:
// MSVC: catchpad within
// MSVC: call ptr @llvm.eh.exceptionpointer.p0(token
// MSVC: br label %catch.throws
// MSVC: catch.throws:
// MSVC: call void @"??1error@std@@QEAA@XZ"(ptr {{.*}} %e)

// Wasm uses funclet EH with wasm.get.exception storing the object in exn.slot.
// WASM: define void @_Z3barv() #[[ATTR:[0-9]+]] personality ptr @__gxx_wasm_personality_v0 {
// WASM: invoke void @_Z3foov()
// WASM:         to label %invoke.cont unwind label %catch.dispatch
// WASM: catchswitch within none [label %catch.start]
// WASM: catchpad within
// WASM: call ptr @llvm.wasm.get.exception(token
// WASM: herb.legacy.convert:
// WASM: load ptr, ptr %exn.slot
// WASM: br label %catch.throws

// SjLj uses a landingpad plus __cxa_get_exception_ptr.
// SJLJ: define dso_local void @_Z3barv() #[[ATTR:[0-9]+]] personality ptr @__gxx_personality_sj0 {
// SJLJ: invoke void @_Z3foov()
// SJLJ: herb.legacy.convert:
// SJLJ: call ptr @__cxa_get_exception_ptr(ptr %exn)
// SJLJ: br label %catch.throws

// ITANIUM: attributes #[[ATTR]] = { {{.*}} }
