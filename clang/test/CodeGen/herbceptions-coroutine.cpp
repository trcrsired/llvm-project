// RUN: %clang -std=c++20 -fherbceptions -fno-exceptions -S -emit-llvm -o - %s | FileCheck %s

// A non-throws coroutine still compiles with -fherbceptions, and a 'throws'
// coroutine wraps its body so that an escaping herbception is delivered to
// promise.unhandled_herbception(__cxx_std_error).
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

// CHECK: define{{.*}} @_Z9make_taski
// CHECK: coroutine
Task make_task(int x) {
  co_return;
}

// 'throws' coroutine: the body is wrapped in a catch-throws handler so an
// escaping error reaches promise.unhandled_herbception, and the return
// union/discriminant stay on the ramp's stack (coro.outside.frame).
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

extern void some_throws() throws;

// CHECK: define {{.*}} @_Z9herb_taskDri
// CHECK: coro.outside.frame
// CHECK: unhandled_herbception
HerbTask herb_task(int x) throws {
  some_throws();
  co_return;
}

int main() {
  return 0;
}
