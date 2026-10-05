// RUN: %clang -std=c++20 -fherbceptions -fcxx-exceptions -S -emit-llvm -o - %s | FileCheck %s

// A 'throws' coroutine must also catch legacy C++ exceptions escaping its
// body: the synthesized handler converts them to a __cxx_std_error in the
// itanium exception_ptr domain and delivers it to
// promise.unhandled_herbception(__cxx_std_error), the same hook used for
// herbception errors.
// Self-contained: defines coroutine machinery manually.

namespace std {
template <typename T, typename... Args>
struct coroutine_traits {};
struct suspend_never {
  constexpr bool await_ready() const noexcept { return true; }
  constexpr void await_suspend(auto) const noexcept {}
  constexpr void await_resume() const noexcept {}
};
template <typename Promise = void>
struct coroutine_handle {
  constexpr coroutine_handle() noexcept = default;
  static coroutine_handle from_address(void* addr) { return {}; }
};
}

struct HerbTask {
  struct promise_type {
    __cxx_std_error error{};
    HerbTask get_return_object() { return {}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_herbception(__cxx_std_error e) noexcept { error = e; }
    void unhandled_exception() {}
  };
};

template <>
struct std::coroutine_traits<HerbTask, int> {
  using promise_type = HerbTask::promise_type;
};

void throws_legacy() { throw int{5}; }

// CHECK-LABEL: define {{.*}} @_Z9herb_taskDri(
// The body runs inside a try so the legacy throw unwinds to a landingpad...
// CHECK: invoke void @_Z13throws_legacyv()
// CHECK: landingpad { ptr, i32 }
// ...whose handler boxes the exception pointer into a cxx_std_error in the
// itanium exception_ptr domain...
// CHECK: call ptr @__cxa_error_domain_itanium_exception_ptr()
// CHECK: call i64 @__cxa_error_code_itanium_exception_ptr(
// ...and passes the converted error to the herbceptions hook.
// CHECK: call void @_ZN8HerbTask12promise_type21unhandled_herbceptionE15__cxx_std_error(
HerbTask herb_task(int x) throws {
  throws_legacy();
  co_return;
}
