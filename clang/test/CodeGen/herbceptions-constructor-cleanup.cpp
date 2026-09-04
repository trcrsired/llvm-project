// clang-format off
// RUN: %clang_cc1 -std=c++20 -fherbceptions -emit-llvm -o - %s | FileCheck %s
// clang-format on

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
};
} // namespace std

struct Base {
  Base();
  ~Base();
};

struct Member {
  Member();
  ~Member();
};

struct FallibleMember {
  FallibleMember() throws;
  ~FallibleMember();
};

struct Partial : Base {
  Member completed;
  FallibleMember fallible;
  Partial() throws;
};

Partial::Partial() throws : Base(), completed(), fallible() {}

// A deterministic failure from the final member constructor returns through
// the ordinary cleanup graph.  The completed member and base must therefore
// have discriminator-guarded normal cleanups in reverse construction order.
// CHECK-LABEL: define {{.*}} @_ZN7PartialC2Ev(
// CHECK: call { { ptr, i64 }, i1 } @_ZN14FallibleMemberC1Ev(
// CHECK: extractvalue { { ptr, i64 }, i1 } {{.*}}, 1
// CHECK: br i1 {{.*}}, label %herb.implicit.err, label %herb.implicit.ok
// CHECK: load i1, ptr %herbception.disc
// CHECK: herb.ctor.partial.destroy:
// CHECK: call void @_ZN6MemberD1Ev(
// CHECK: herb.ctor.partial.destroy{{.*}}:
// CHECK: call void @_ZN4BaseD2Ev(
