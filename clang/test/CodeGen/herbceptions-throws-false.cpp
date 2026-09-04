// RUN: %clang --target=x86_64-unknown-linux-gnu -std=c++20 \
// RUN:   -fherbceptions -fno-exceptions -fms-extensions -S -emit-llvm -o - %s \
// RUN:   | FileCheck %s

// `throws(false)` is the non-failing state of a conditional specification. It
// is ABI-identical to noexcept(true), so neither definitions nor calls may use
// the herbception `{payload, i1}` return or the LLVM `throws` attribute.
// CHECK-LABEL: define dso_local noundef i32 @_Z8disabledi(i32 noundef %{{.*}})
// CHECK-LABEL: define dso_local noundef i32 @_Z13call_disabledi(i32 noundef %{{.*}})
// CHECK: call noundef i32 @_Z8disabledi
int disabled(int value) throws(false) { return value + 1; }
int call_disabled(int value) throws(false) { return disabled(value); }

// A live channel remains ABI-distinct and automatically propagates in a live
// caller. This guards against canonicalizing all `throws(bool)` spellings to
// noexcept instead of canonicalizing only the false state.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z7enabledi(i32 noundef %{{.*}})
// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z12call_enabledi(i32 noundef %{{.*}})
// CHECK: call { { ptr, i64 }, i1 } @_Z7enabledi
// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z12enabled_truei(i32 noundef %{{.*}})
int enabled(int value) throws { return value + 2; }
int call_enabled(int value) throws { return enabled(value); }
int enabled_true(int value) throws(true) { return value + 3; }

alignas(32) int global_value;

// Return attributes derived from the logical pointer/reference result belong
// to the success payload. The shaped aggregate itself is neither a pointer nor
// guaranteed to contain an active success value, so such attributes would be
// invalid on the LLVM return and are deliberately absent below.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z16reference_resultv()
int &reference_result() throws { return global_value; }

// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z14nonnull_resultv()
__attribute__((returns_nonnull)) int *nonnull_result() throws {
  return &global_value;
}

// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z17restricted_resultv()
__declspec(restrict) int *restricted_result() throws { return &global_value; }

// In the disabled state the LLVM result really is a reference pointer, so the
// usual pointer return contracts remain valid rather than being globally
// suppressed for the source spelling `throws`.
// CHECK-LABEL: define dso_local noundef nonnull align 4 dereferenceable(4) ptr @_Z18disabled_referencev()
int &disabled_reference() throws(false) { return global_value; }
