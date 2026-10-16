// RUN: %clang_cc1 -std=c++20 -fherbceptions -fcxx-exceptions -fsyntax-only -verify %s

// A coroutine may declare 'throws': herbception errors escaping the body are
// delivered to promise_type::unhandled_herbception(__cxx_std_error), and
// ramp-phase failures propagate to the caller through the channel. The
// promise must declare unhandled_herbception; unhandled_exception is optional
// in that mode. A coroutine without 'throws' still requires
// unhandled_exception().

namespace std {
template <typename... T> struct coroutine_traits;
template <class Promise = void> struct coroutine_handle {
  coroutine_handle() = default;
  static coroutine_handle from_address(void *) noexcept;
};
template <> struct coroutine_handle<void> {
  static coroutine_handle from_address(void *) noexcept;
  coroutine_handle() = default;
  template <class PromiseType>
  coroutine_handle(coroutine_handle<PromiseType>) noexcept;
};
struct suspend_never {
  bool await_ready() noexcept { return true; }
  void await_suspend(coroutine_handle<>) noexcept {}
  void await_resume() noexcept {}
};
// the error channel type a `catch throws` handler binds
struct error {
  void *domain;
  unsigned long code;
};
} // namespace std

// promise with both hooks
struct Task {
  struct promise_type {
    Task get_return_object() { return {}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_herbception(__cxx_std_error) noexcept {}
    void unhandled_exception() {}
  };
};

template <>
struct std::coroutine_traits<Task, int> {
  using promise_type = Task::promise_type;
};

// promise with only unhandled_herbception: unhandled_exception is not
// required for a 'throws' coroutine.
struct TaskHerbOnly {
  // expected-note@+1 {{'promise_type' defined here}}
  struct promise_type {
    TaskHerbOnly get_return_object() { return {}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_herbception(__cxx_std_error) noexcept {}
  };
};

template <>
struct std::coroutine_traits<TaskHerbOnly, int> {
  using promise_type = TaskHerbOnly::promise_type;
};

// promise without unhandled_herbception: usable only for non-'throws'
// coroutines.
struct TaskNoHook {
  // expected-note@+1 {{'promise_type' defined here}}
  struct promise_type {
    TaskNoHook get_return_object() { return {}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
  };
};

template <>
struct std::coroutine_traits<TaskNoHook, int> {
  using promise_type = TaskNoHook::promise_type;
};

// 'throws' coroutine: OK when the promise declares unhandled_herbception.
Task make_task(int x) throws {
  co_return;
}

// 'throws' coroutine: unhandled_exception is not required.
TaskHerbOnly make_task_herb_only(int x) throws {
  co_return;
}

// expected-error@+1 {{'throws' coroutine requires member 'unhandled_herbception' in promise type}}
TaskNoHook make_task_no_hook(int x) throws {
  co_return;
}

// A non-'throws' coroutine still requires unhandled_exception().
// expected-error@+1 {{'promise_type' is required to declare the member 'unhandled_exception()'}}
TaskHerbOnly make_task_plain(int x) {
  co_return;
}

// A `catch throws` handler is not an unwinding handler: the error has already
// been returned to the caller, so suspending inside it is as safe as inside a
// plain block, and the handler restriction of [expr.await]p2 does not apply.
// Regression test: a missing Herbception carve-out made Sema reject this.
Task await_in_catch_throws(int x) throws {
  try {
    co_await std::suspend_never{};
  } catch throws(std::error e) {
    co_await std::suspend_never{};
    (void)e.code;
  }
  co_return;
}

// A legacy C++ exception handler may run while the stack unwinds, so it still
// may not suspend — the carve-out is only for `catch throws`.
Task await_in_cxx_catch(int x) {
  try {
    co_await std::suspend_never{};
  } catch (...) {
    // expected-error@+1 {{'co_await' cannot be used in the handler of a try block}}
    co_await std::suspend_never{};
  }
  co_return;
}
