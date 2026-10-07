// RUN: %clang_cc1 -std=c++26 -fherbceptions -fcxx-exceptions -fexceptions -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fcxx-exceptions -fexceptions -fclangir -emit-llvm -o - %s | FileCheck %s --check-prefix=CIR

// A 'return_failure{E}' function is NOT noexcept (it may fail via the
// herbceptions channel), but it is 'nounwind' in LLVM IR (no unwinding).
// Any legacy C++ exception escaping it calls std::terminate (a terminate
// landing pad is pushed). 'return_failure{E}' and 'noexcept' cannot be
// combined: throws supersedes noexcept.

using size_t = __SIZE_TYPE__;
struct E { int code; };
struct S { int v; };

extern void legacy();

// CHECK: define dso_local { i32, i1 } @_Z8defaultfDE1EEv() #[[DEF:[0-9]+]] personality ptr @__gxx_personality_v0 {
// CHECK: invoke void @_Z6legacyv()
// CHECK:         to label %invoke.cont unwind label %terminate.lpad
// CHECK: terminate.lpad:
// CHECK: call void @__clang_call_terminate
// CIR: define dso_local { i32, i8 } @_Z8defaultfDE1EEv() #[[CIRDEF:[0-9]+]] personality ptr @__gxx_personality_v0 {
// CIR: invoke void @_Z6legacyv()
// CIR:         to label %{{.*}} unwind label %{{.*}}
// CIR: landingpad { ptr, i32 }
// CIR:         catch ptr null
// CIR: call void @__clang_call_terminate
int defaultf() return_failure{E} {
  legacy();
  return 1;
}

// CHECK: attributes #[[DEF]] = { {{.*}} nounwind {{.*}} throws {{.*}} }
// CIR: attributes #[[CIRDEF]] = { {{.*}}nounwind throws{{.*}} }
