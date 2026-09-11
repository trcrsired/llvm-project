// RUN: %clang -fherbceptions -fno-exceptions -S -emit-llvm -o - %s | FileCheck %s

// A return_failure error value is stored through the same slot the success
// value uses. That slot is typed as the union of the two, which is the payload
// when the payload is at least as large as the error -- and the payload need
// not be a struct. With an i64-sized payload and an 8-byte struct error the
// slot is therefore a plain i64, and emitting the aggregate error straight into
// it would GEP a field out of a non-struct address. The aggregate has to be
// built in a temp of its own type and its bytes copied over.

using W = __SIZE_TYPE__;
struct E1 { W x; };

// CHECK: define dso_local { i64, i1 } @_Z1fi(i32
// CHECK:       %[[SLOT:.*]] = alloca i64, align 8
// CHECK:       %[[ERR:.*]] = alloca %struct.E1, align 8
// The error is built in its own storage...
// CHECK:       getelementptr inbounds nuw %struct.E1, ptr %[[ERR]]
// ...and copied into the slot, limited to the slot's width.
// CHECK:       call void @llvm.memcpy{{.*}}(ptr {{.*}}%[[SLOT]], ptr {{.*}}%[[ERR]], i64 8
W f(int n) return_failure{E1} {
  if (n < 0)
    return_failure(E1{(W)-n});
  return (W)n;
}
