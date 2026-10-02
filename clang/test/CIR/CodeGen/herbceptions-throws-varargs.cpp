// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fherbceptions -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s

// A variadic function may be declared 'throws': its signature returns the
// shaped {E, i1} result while the ellipsis is preserved. Both direct and
// indirect call sites keep the written wire form (the discriminant travels
// out of band), and va_arg still works inside the function.

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
};
}

int sum_throws(int n, ...) throws;

// Direct call to a variadic throws callee.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z3useDrv()
// CHECK:         call { { ptr, i64 }, i8 } (i32, ...) @_Z10sum_throwsDriz(i32 noundef 3, i32 noundef 10, i32 noundef 20, i32 noundef 30)
int use() throws {
  return sum_throws(3, 10, 20, 30);
}

// CHECK-LABEL: declare { { ptr, i64 }, i8 } @_Z10sum_throwsDriz(i32 noundef, ...)

// Indirect call through a pointer to a variadic throws function.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z7via_ptrDrPDrFiizEii(
// CHECK:         call { { ptr, i64 }, i8 } (i32, ...) %
int via_ptr(int (*fp)(int, ...) throws, int n, int a) throws {
  return fp(n, a);
}

// va_start/va_arg inside a throws function lower as usual and the shaped
// result is returned.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z9count_maxDriz(i32 noundef {{.*}}, ...)
// CHECK:         call void @llvm.va_start.p0
// CHECK:         call void @llvm.va_end.p0
// CHECK:         ret { { ptr, i64 }, i8 }
int count_max(int n, ...) throws {
  __builtin_va_list ap;
  __builtin_va_start(ap, n);
  int m = 0;
  for (int i = 0; i < n; ++i) {
    int v = __builtin_va_arg(ap, int);
    if (v > m)
      m = v;
  }
  __builtin_va_end(ap);
  return m;
}
