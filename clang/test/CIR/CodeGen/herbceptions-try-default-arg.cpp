// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm %s -o - | FileCheck %s

// A throws call in the default argument of a throws function template is
// wrapped in an implicit try() when the default argument is instantiated --
// in the *declared* function's context. The try node is then emitted in the
// caller's frame, which need not be a throws function: in main() the error
// traps, in a `catch throws` scope it reaches the handler, and in a throws
// function it auto-propagates. Emitting it must not touch the caller's
// (nonexistent) herbception return slots.

namespace std {
struct error {
  void *d;
  __SIZE_TYPE__ c;
};
}

bool daylight() throws;

template <long X>
int local(int ts, bool dl = daylight()) throws {
  return ts;
}

// The bare call's default-arg error traps in main; the call inside try{}
// routes to the catch throws handler instead.
// CHECK-LABEL: define dso_local noundef i32 @main()
// CHECK: call { { ptr, i64 }, i8 } @_Z8daylightDrv()
// CHECK: call void @llvm.trap()
// CHECK: {{call|invoke}} { { ptr, i64 }, i8 } @_Z8daylightDrv()
// CHECK: store %"struct.std::error"
int main() {
  int r = local<0>(0);
  try {
    r += local<0>(0);
  } catch throws(std::error e) {
    r = -1;
  }
  return r;
}

// In a throws caller the same default argument auto-propagates.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z12caller_outerDrv(
// CHECK: call { { ptr, i64 }, i8 } @_Z8daylightDrv()
// CHECK: ret { { ptr, i64 }, i8 }
int caller_outer() throws {
  return local<0>(0);
}
