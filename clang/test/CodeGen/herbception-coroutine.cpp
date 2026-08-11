// RUN: %clang -std=c++20 -fherbceptions -fno-exceptions -S -emit-llvm -o - %s | FileCheck %s

// Herbception throws on a coroutine: the ramp function returns {Task, i1}
// with the 'throws' attribute, and the discriminant is kept on the stack (not
// in the coroutine frame, which is deallocated before the ramp reads it).

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

// CHECK: define dso_local { %struct.Task, i1 } @_Z9make_taski(i32 noundef %x) #[[ATTR:[0-9]+]]
// CHECK: %herbception.disc = alloca i1, align 1, !coro.outside.frame
Task make_task(int x) throws {
  if (x < 0) throw throws x;
  co_return;
}

// The caller extracts the {T, i1} discriminant (via the carry flag on x86).
// CHECK-LABEL: define dso_local noundef i32 @main
// CHECK: call { %struct.Task, i1 } @_Z9make_taski
int main() {
  auto e = catch fails(make_task(5));
  return e.positive ? 0 : 1;
}

// CHECK: attributes #[[ATTR]] = { {{.*}}throws{{.*}} }
