// RUN: %clang --target=x86_64-unknown-linux-gnu -std=c++20 \
// RUN:   -fherbceptions -fno-exceptions -S -emit-llvm -o - %s \
// RUN:   | FileCheck %s

template <bool Enabled>
__attribute__((noinline)) int conditional(int value) throws(Enabled) {
  return value + 1;
}

template int conditional<false>(int);
template int conditional<true>(int);

// The disabled specialization and its caller use the ordinary scalar return.
// CHECK-LABEL: define weak_odr dso_local noundef i32 @_Z11conditionalILb0EEii(i32 noundef %{{.*}})
// CHECK-LABEL: define weak_odr dso_local { { ptr, i64 }, i1 } @_Z11conditionalILb1EEii(i32 noundef %{{.*}})

int global;

template <bool Enabled>
__attribute__((noinline)) int &conditional_ref() throws(Enabled) {
  return global;
}

template int &conditional_ref<false>();
template int &conditional_ref<true>();

// A false reference specialization retains the normal pointer-return ABI and
// its valid reference contracts. Only the true specialization is shaped.
// CHECK-LABEL: define weak_odr dso_local noundef nonnull align 4 dereferenceable(4) ptr @_Z15conditional_refILb0EERiv()
// CHECK-LABEL: define weak_odr dso_local { { ptr, i64 }, i1 } @_Z15conditional_refILb1EERiv()

// CHECK-LABEL: define dso_local noundef i32 @_Z13call_disabledi(i32 noundef %{{.*}})
// CHECK: call noundef i32 @_Z11conditionalILb0EEii
// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z12call_enabledi(i32 noundef %{{.*}})
// CHECK: call { { ptr, i64 }, i1 } @_Z11conditionalILb1EEii
int call_disabled(int value) throws(false) {
  return conditional<false>(value);
}

int call_enabled(int value) throws {
  return conditional<true>(value);
}

__attribute__((noinline)) int always_fallible(int value) throws {
  return value + 2;
}

// A non-dependent fallible call is not rebuilt during template instantiation.
// It must therefore be represented as potential propagation while parsing the
// dependent throws body. The discarded false branch keeps that specialization
// valid and verifies that its function ABI remains ordinary.
template <bool Enabled>
__attribute__((noinline)) int forward_nondependent(int value) throws(Enabled) {
  if constexpr (Enabled)
    return always_fallible(value);
  return value;
}

template int forward_nondependent<false>(int);
template int forward_nondependent<true>(int);

// CHECK-LABEL: define weak_odr dso_local noundef i32 @_Z20forward_nondependentILb0EEii(i32 noundef %{{.*}})
// CHECK-NOT: call { { ptr, i64 }, i1 } @_Z15always_falliblei
// CHECK-LABEL: define weak_odr dso_local { { ptr, i64 }, i1 } @_Z20forward_nondependentILb1EEii(i32 noundef %{{.*}})
// CHECK: call { { ptr, i64 }, i1 } @_Z15always_falliblei

// Explicit try(expr) likewise has to be accepted in the potential context and
// rebuilt only after the enclosing condition has been resolved.
template <bool Enabled>
__attribute__((noinline)) int forward_explicit(int value) throws(Enabled) {
  if constexpr (Enabled)
    return try(always_fallible(value));
  return value;
}

template int forward_explicit<false>(int);
template int forward_explicit<true>(int);

// CHECK-LABEL: define weak_odr dso_local noundef i32 @_Z16forward_explicitILb0EEii(i32 noundef %{{.*}})
// CHECK-LABEL: define weak_odr dso_local { { ptr, i64 }, i1 } @_Z16forward_explicitILb1EEii(i32 noundef %{{.*}})
// CHECK: call { { ptr, i64 }, i1 } @_Z15always_falliblei

// A dependent callee is rebuilt after substitution. Both conditions resolve
// together, yielding an ordinary call in the false specialization and an
// automatically propagating shaped call in the true specialization.
template <bool Enabled>
__attribute__((noinline)) int forward_dependent(int value) throws(Enabled) {
  return conditional<Enabled>(value);
}

template int forward_dependent<false>(int);
template int forward_dependent<true>(int);

// CHECK-LABEL: define weak_odr dso_local noundef i32 @_Z17forward_dependentILb0EEii(i32 noundef %{{.*}})
// CHECK: call noundef i32 @_Z11conditionalILb0EEii
// CHECK-LABEL: define weak_odr dso_local { { ptr, i64 }, i1 } @_Z17forward_dependentILb1EEii(i32 noundef %{{.*}})
// CHECK: call { { ptr, i64 }, i1 } @_Z11conditionalILb1EEii
