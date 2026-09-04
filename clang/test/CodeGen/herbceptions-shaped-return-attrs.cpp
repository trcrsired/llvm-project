// RUN: %clang --target=x86_64-unknown-linux-gnu -std=c++20 \
// RUN:   -fherbceptions -fno-exceptions -S -emit-llvm -o - %s \
// RUN:   | FileCheck %s
// RUN: %clang --target=x86_64-unknown-linux-gnu -std=c++20 \
// RUN:   -fherbceptions -fno-exceptions -S -emit-llvm -o - %s \
// RUN:   | opt -passes=verify -disable-output

using size_t = decltype(sizeof(0));

// Source return contracts describe only the pointer in the success payload.
// They cannot be represented as attributes on `{payload, i1}`, and assuming
// them before testing the discriminant would be false on the failure path.
__attribute__((assume_aligned(64))) int *aligned_result() throws {
  static int value;
  return &value;
}

__attribute__((alloc_align(1))) void *alloc_aligned(size_t alignment) throws {
  (void)alignment;
  static int value;
  return &value;
}

__attribute__((alloc_size(1))) void *sized_result(size_t size) throws {
  (void)size;
  static int value;
  return &value;
}

// CHECK-NOT: allocsize
// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z12call_alignedv()
// CHECK: call { { ptr, i64 }, i1 } @_Z14aligned_resultv()
// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z18call_alloc_alignedv()
// CHECK: call { { ptr, i64 }, i1 } @_Z13alloc_alignedm(i64 noundef 64)
// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z10call_sizedv()
// CHECK: call { { ptr, i64 }, i1 } @_Z12sized_resultm(i64 noundef 64)
// CHECK-NOT: allocsize
int *call_aligned() throws { return aligned_result(); }
void *call_alloc_aligned() throws { return alloc_aligned(64); }
void *call_sized() throws { return sized_result(64); }
