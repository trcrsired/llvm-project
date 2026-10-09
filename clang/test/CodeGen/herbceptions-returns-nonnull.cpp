// RUN: %clang -fherbceptions -fno-exceptions -O1 -fsanitize=undefined -S -emit-llvm -o - %s | FileCheck %s

// The LLVM return value of a throws/fails function is the error channel
// ({E, i1} aggregate or a lone i1 discriminant), not the C++ return value.
// A returns_nonnull / _Nonnull annotation applies to that C++ value, so it
// must not produce a nonnull return attribute or a ubsan null check on the
// aggregate (icmp on an aggregate type is invalid IR).

extern "C" void *malloc(unsigned long);

// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z10alloc_likeDrm(
// CHECK-NOT:     icmp eq { { ptr, i64 }, i1 } %{{.*}}, zeroinitializer
// CHECK-NOT:     @__ubsan_handle_nonnull_return_v1
// CHECK:         ret { { ptr, i64 }, i1 } %{{.*}}
__attribute__((returns_nonnull)) void *alloc_like(unsigned long n) throws {
  return malloc(n);
}

// Sanity: the same annotation on a plain function still produces the check.
// CHECK-LABEL: define dso_local {{.*}}nonnull{{.*}} ptr @_Z11alloc_plainm(
// CHECK:         icmp eq ptr %{{.*}}, null
// CHECK:         call void @__ubsan_handle_nonnull_return_v1
// CHECK:         ret ptr
__attribute__((returns_nonnull)) void *alloc_plain(unsigned long n) {
  return malloc(n);
}
