// RUN: %clang_cc1 -std=c++20 -fherbceptions -fno-cxx-exceptions -O0 -emit-llvm -o - %s | FileCheck %s

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
};
} // namespace std

struct PlainBase {
  PlainBase(int, ...) throws(false);
  ~PlainBase();
};

struct PlainMember {
  PlainMember();
  ~PlainMember();
};

struct PlainDerived : PlainBase {
  using PlainBase::PlainBase;
  PlainMember member;
};

int fallibleLeaf() throws;

// The enclosing function is active, but the variadic `throws(false)`
// constructor being emitted inline is ordinary. Its successful base/member
// initialization must not acquire discriminator-guarded partial-construction
// cleanups.
int ordinaryInActive() throws {
  PlainDerived value(1, 2);
  return fallibleLeaf();
}

// CHECK-LABEL: define {{.*}} @{{.*}}ordinaryInActive{{.*}}(
// CHECK-NOT: herb.ctor.partial.destroy
// CHECK: call void (ptr, i32, ...) @{{.*}}PlainBaseC{{[12]}}{{.*}}(
// CHECK: call void @{{.*}}PlainMemberC{{[12]}}{{.*}}(
// CHECK-NOT: herb.ctor.partial.destroy
// CHECK: call {{.*}} @{{.*}}fallibleLeaf{{.*}}(
// CHECK-NOT: herb.ctor.partial.destroy
// CHECK: ret

struct ActiveBase {
  ActiveBase(int, ...) throws;
  ~ActiveBase();
};

struct ActiveMember {
  ActiveMember() throws;
  ~ActiveMember();
};

struct ActiveDerived : ActiveBase {
  using ActiveBase::ActiveBase;
  ActiveMember member;
};

// Both calls below are emitted inside the inherited constructor's nested
// cleanup scope. The ambient return carrier must remain usable, and a member
// failure after a successful base construction must cross the base cleanup.
int activeInActive() throws {
  ActiveDerived value(1, 2);
  return 5;
}

// CHECK-LABEL: define {{.*}} @{{.*}}activeInActive{{.*}}(
// CHECK: call {{.*}} @{{.*}}ActiveBaseC{{[12]}}{{.*}}(
// CHECK: call {{.*}} @{{.*}}ActiveMemberC{{[12]}}{{.*}}(
// CHECK: call void @{{.*}}ActiveBaseD2{{.*}}(
// CHECK: ret

int activeInPlain() {
  try {
    ActiveDerived value(1, 2);
    return 5;
  } catch throws(std::error Error) {
    return static_cast<int>(Error.code);
  }
}

// CHECK-LABEL: define {{.*}} @{{.*}}activeInPlain{{.*}}(
// CHECK: call {{.*}} @{{.*}}ActiveBaseC{{[12]}}{{.*}}(
// CHECK: call {{.*}} @{{.*}}ActiveMemberC{{[12]}}{{.*}}(
// CHECK: call void @{{.*}}ActiveBaseD2{{.*}}(
// CHECK: ret i32
