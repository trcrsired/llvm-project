// RUN: %clang_cc1 -std=c++26 -fherbceptions -fcxx-exceptions -fexceptions -emit-llvm -o - %s | FileCheck %s

// A default 'fails{E}' function implies noexcept(true): any legacy C++
// exception escaping it calls std::terminate (a terminate landing pad is
// pushed, exactly like a noexcept function). A 'fails{E} noexcept(false)'
// function instead allows traditional C++ exceptions to propagate, so it has
// no terminate scope.

using size_t = __SIZE_TYPE__;
struct E { int code; };
struct S { int v; };

extern void legacy();

// CHECK: define dso_local { i32, i1 } @_Z8defaultfv() #[[DEF:[0-9]+]] personality ptr @__gxx_personality_v0 {
// CHECK: invoke void @_Z6legacyv()
// CHECK:         to label %invoke.cont unwind label %terminate.lpad
// CHECK: terminate.lpad:
// CHECK: call void @__clang_call_terminate
int defaultf() fails{E} {
  legacy();
  return 1;
}

// CHECK: define dso_local { i32, i1 } @_Z8nofalsefv() #[[NF:[0-9]+]] {
// CHECK: call void @_Z6legacyv()
int nofalsef() fails{E} noexcept(false) {
  legacy();
  return 1;
}

// CHECK: attributes #[[DEF]] = { {{.*}} nounwind {{.*}} throws {{.*}} }
// CHECK: attributes #[[NF]] = { {{.*}} throws {{.*}} }
