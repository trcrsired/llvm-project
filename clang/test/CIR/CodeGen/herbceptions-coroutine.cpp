// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fherbceptions -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fherbceptions -fcxx-exceptions -fexceptions -fclangir -emit-cir %s -o %t.legacy.cir
// RUN: FileCheck --input-file=%t.legacy.cir %s -check-prefix=LEGACY

// A non-throws coroutine still compiles with -fherbceptions, and a 'throws'
// coroutine wraps its body so that an escaping herbception is delivered to
// promise.unhandled_herbception(__cxx_std_error). Ramp-phase failures return
// through the shaped {payload, i1} result instead. When legacy C++ exceptions
// are enabled, an escaping one is converted to an itanium exception_ptr error
// and delivered through the same hook.
// Self-contained: defines coroutine machinery manually.

namespace std {
template <typename T, typename... Args>
struct coroutine_traits {};
struct suspend_never {
  constexpr bool await_ready() const noexcept { return true; }
  constexpr void await_suspend(auto) const noexcept {}
  constexpr void await_resume() const noexcept {}
};
struct suspend_always {
  constexpr bool await_ready() const noexcept { return false; }
  constexpr void await_suspend(auto) const noexcept {}
  constexpr void await_resume() const noexcept {}
};
template <typename Promise = void>
struct coroutine_handle {
  constexpr coroutine_handle() noexcept = default;
  template <typename P>
  coroutine_handle(coroutine_handle<P>) noexcept {}
  static coroutine_handle from_address(void* addr) { return {}; }
  constexpr void* address() const noexcept { return nullptr; }
  constexpr bool done() const noexcept { return true; }
  constexpr void resume() const {}
  constexpr void destroy() const {}
  constexpr operator bool() const noexcept { return true; }
};
struct noop_coroutine_promise {};
using noop_coroutine_handle = coroutine_handle<noop_coroutine_promise>;
noop_coroutine_handle noop_coroutine() noexcept { return {}; }
}

struct Task {
  struct promise_type {
    Task get_return_object() { return {}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
  };
};

template <>
struct std::coroutine_traits<Task, int> {
  using promise_type = Task::promise_type;
};

// CIR-LABEL: cir.func coroutine{{.*}} @_Z9make_taski
Task make_task(int x) {
  co_return;
}

// 'throws' coroutine: the body is wrapped in a catch-throws handler so an
// escaping error reaches promise.unhandled_herbception.
struct HerbTask {
  struct promise_type {
    __cxx_std_error error{};
    HerbTask get_return_object() { return {}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_herbception(__cxx_std_error e) noexcept { error = e; }
  };
};

template <>
struct std::coroutine_traits<HerbTask, int> {
  using promise_type = HerbTask::promise_type;
};

extern void some_throws() throws;

// CIR-LABEL: cir.func coroutine{{.*}} @_Z9herb_taskDri
// CIR: cir.throws
HerbTask herb_task(int x) throws {
  // CIR: cir.try {
  // CIR: cir.call @_Z11some_throwsDrv() {cir.throws}
  // CIR: cir.goto "catch.throws
  some_throws();
  co_return;
}

// CIR: cir.label "catch.throws
// CIR: cir.call @_ZN8HerbTask12promise_type21unhandled_herbceptionE15__cxx_std_error

// A ramp-phase herbception (here a throws copy constructor for the parameter
// copied into the frame) must return through the shaped result, not be
// delivered to unhandled_herbception: it happens before the coroutine body.
struct ThrowingParam {
  ThrowingParam() = default;
  ThrowingParam(ThrowingParam const &) throws;
};

template <>
struct std::coroutine_traits<HerbTask, ThrowingParam> {
  using promise_type = HerbTask::promise_type;
};

// CIR-LABEL: cir.func coroutine{{.*}} @_Z10herb_paramDr13ThrowingParam
// CIR: cir.call @_ZN13ThrowingParamC1EDrRKS_({{.*}}) {cir.throws}
// CIR: cir.if
// CIR: cir.return
HerbTask herb_param(ThrowingParam p) throws { co_return; }

// A void-returning throws coroutine has no return object; the ramp still
// returns the shaped {error, i1} result with a null error payload.
namespace std {
template <typename... T>
struct coroutine_traits<void, T...> {
  struct promise_type {
    void get_return_object() {}
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_herbception(__cxx_std_error) noexcept {}
  };
};
}

// CIR-LABEL: cir.func coroutine{{.*}} @_Z9herb_voidDri
// CIR: cir.return {{.*}} : !rec_anon_struct
void herb_void(int x) throws {
  co_return;
}

#if defined(__cpp_exceptions)
void throws_legacy() { throw int{5}; }

// A legacy C++ exception escaping the body unwinds to the synthesized
// catch-all, which mints an itanium exception_ptr error and passes it to the
// herbception hook.
// LEGACY-LABEL: cir.func coroutine{{.*}} @_Z16herb_task_legacyDri
// LEGACY: cir.throws
HerbTask herb_task_legacy(int x) throws {
  // LEGACY: cir.try {
  // LEGACY: cir.call @_Z13throws_legacyv()
  // LEGACY: } catch all (
  // LEGACY: cir.eh.get_exception_ptr
  // LEGACY: cir.call @__cxa_error_domain_itanium_exception_ptr()
  // LEGACY: cir.call @__cxa_error_code_itanium_exception_ptr(
  throws_legacy();
  co_return;
}

// LEGACY: cir.call @_ZN8HerbTask12promise_type21unhandled_herbceptionE15__cxx_std_error
#endif

int main() {
  return 0;
}
