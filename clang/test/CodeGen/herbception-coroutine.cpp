// RUN: %clang -std=c++20 -fherbceptions -fno-exceptions -S -emit-llvm -o - %s | FileCheck %s

// Coroutines must not use 'throws'. This test verifies that a non-throws
// coroutine still compiles correctly with -fherbceptions.

#include <coroutine>

struct Task {
  struct promise_type {
    Task get_return_object() { return {}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
  };
};

// CHECK: define{{.*}} @_Z9make_taski
Task make_task(int x) {
  co_return;
}

int main() {
  return 0;
}
