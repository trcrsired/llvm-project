// RUN: %clang -std=c++20 -fherbceptions -fno-exceptions -S -emit-llvm -o - %s | FileCheck %s

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
};
}

struct Active {
  Active() throws;
  Active(const Active &) throws;
  Active &operator=(const Active &) throws;
  int value;
};

struct Legacy {
  Legacy() noexcept(false);
  Legacy(const Legacy &) noexcept(false);
  Legacy &operator=(const Legacy &) noexcept(false);
  int value;
};

struct ActiveThenLegacy {
  ActiveThenLegacy() = default;
  ActiveThenLegacy(const ActiveThenLegacy &) = default;
  ActiveThenLegacy &operator=(const ActiveThenLegacy &) = default;
  Active active;
  Legacy legacy;
};

struct LegacyThenActive {
  LegacyThenActive() = default;
  LegacyThenActive(const LegacyThenActive &) = default;
  LegacyThenActive &operator=(const LegacyThenActive &) = default;
  Legacy legacy;
  Active active;
};

int make_active_then_legacy() throws {
  ActiveThenLegacy value;
  return 1;
}

int copy_active_then_legacy(const ActiveThenLegacy &source) throws {
  ActiveThenLegacy value(source);
  return 2;
}

ActiveThenLegacy &assign_active_then_legacy(
    ActiveThenLegacy &target, const ActiveThenLegacy &source) throws {
  return target = source;
}

int make_legacy_then_active() throws {
  LegacyThenActive value;
  return 3;
}

int copy_legacy_then_active(const LegacyThenActive &source) throws {
  LegacyThenActive value(source);
  return 4;
}

LegacyThenActive &assign_legacy_then_active(
    LegacyThenActive &target, const LegacyThenActive &source) throws {
  return target = source;
}

// Both member orders produce the same shaped constructor ABI.  Each active
// subobject call is evaluated once, its discriminator is tested, and the
// ordinary legacy call remains on the corresponding success path.
// CHECK-LABEL: define linkonce_odr dso_local { { ptr, i64 }, i1 } @_ZN16ActiveThenLegacyC2Ev(
// CHECK: call { { ptr, i64 }, i1 } @_ZN6ActiveC1Ev(
// CHECK: extractvalue { { ptr, i64 }, i1 } {{.*}}, 1
// CHECK: br i1 {{.*}}, label %herb.implicit.err, label %herb.implicit.ok
// CHECK: call void @_ZN6LegacyC1Ev(

// CHECK-LABEL: define linkonce_odr dso_local { { ptr, i64 }, i1 } @_ZN16ActiveThenLegacyC2ERKS_(
// CHECK: call { { ptr, i64 }, i1 } @_ZN6ActiveC1ERKS_(
// CHECK: extractvalue { { ptr, i64 }, i1 } {{.*}}, 1
// CHECK: br i1 {{.*}}, label %herb.implicit.err, label %herb.implicit.ok
// CHECK: call void @_ZN6LegacyC1ERKS_(

// CHECK-LABEL: define linkonce_odr dso_local { { ptr, i64 }, i1 } @_ZN16ActiveThenLegacyaSERKS_(
// CHECK: call { { ptr, i64 }, i1 } @_ZN6ActiveaSERKS_(
// CHECK: extractvalue { { ptr, i64 }, i1 } {{.*}}, 1
// CHECK: br i1 {{.*}}, label %herb.implicit.err, label %herb.implicit.ok
// CHECK: call {{.*}} @_ZN6LegacyaSERKS_(

// CHECK-LABEL: define linkonce_odr dso_local { { ptr, i64 }, i1 } @_ZN16LegacyThenActiveC2Ev(
// CHECK: call void @_ZN6LegacyC1Ev(
// CHECK: call { { ptr, i64 }, i1 } @_ZN6ActiveC1Ev(
// CHECK: extractvalue { { ptr, i64 }, i1 } {{.*}}, 1
// CHECK: br i1 {{.*}}, label %herb.implicit.err, label %herb.implicit.ok

// CHECK-LABEL: define linkonce_odr dso_local { { ptr, i64 }, i1 } @_ZN16LegacyThenActiveC2ERKS_(
// CHECK: call void @_ZN6LegacyC1ERKS_(
// CHECK: call { { ptr, i64 }, i1 } @_ZN6ActiveC1ERKS_(
// CHECK: extractvalue { { ptr, i64 }, i1 } {{.*}}, 1
// CHECK: br i1 {{.*}}, label %herb.implicit.err, label %herb.implicit.ok

// CHECK-LABEL: define linkonce_odr dso_local { { ptr, i64 }, i1 } @_ZN16LegacyThenActiveaSERKS_(
// CHECK: call {{.*}} @_ZN6LegacyaSERKS_(
// CHECK: call { { ptr, i64 }, i1 } @_ZN6ActiveaSERKS_(
// CHECK: extractvalue { { ptr, i64 }, i1 } {{.*}}, 1
// CHECK: br i1 {{.*}}, label %herb.implicit.err, label %herb.implicit.ok
