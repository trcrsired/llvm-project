// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm %s -o - | FileCheck %s

// A return_failure error value is stored through the same slot the success
// value uses. That slot is typed as the union of the two -- with an i64-sized
// payload and an 8-byte struct error the slot is a plain i64, so emitting the
// aggregate error straight into it would GEP a field out of a non-struct
// address. The aggregate is built in a temp of its own type and its bytes
// copied over.

using W = __SIZE_TYPE__;
struct E1 { W x; };

// The error is built in its own storage and copied into the slot, limited to
// the slot's width.
// CHECK-LABEL: define dso_local { i64, i8 } @_Z1fDE2E1Ei(i32 noundef %0)
// CHECK: getelementptr inbounds nuw %struct.E1
// CHECK: store i64
// CHECK: store i8 1
// CHECK: ret { i64, i8 }
W f(int n) return_failure{E1} {
  if (n < 0)
    return_failure(E1{(W)-n});
  return (W)n;
}
