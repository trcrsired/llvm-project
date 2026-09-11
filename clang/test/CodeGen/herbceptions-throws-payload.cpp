// RUN: %clang -std=c++20 -fherbceptions -fno-exceptions -S -emit-llvm -o - %s | FileCheck %s

// Herbceptions (throws): the return slot is a union, sized for max(T, E) so it
// can hold either the success value or the error. It is therefore not typed as
// the payload, and anything that constructs or reads the payload has to go
// through the payload's own type. Addressing the payload through the union puts
// its fields at the union's offsets, and the caller reads them back at their
// own -- which silently returns garbage rather than failing to compile.
//
// The struct-field case for a payload built inline is covered by
// herbceptions-throws-sret.cpp (two/three). The two shapes here are the ones
// that reach the slot by other routes.

// Complex. A _Complex float is {float, float}: its imag component belongs at
// offset 4, and lands at offset 8 if the value is built as the union.
//
// CHECK-LABEL: define {{.*}} @_Z2cfi(
// CHECK:         getelementptr inbounds nuw { float, float }, ptr {{.*}}, i32 0, i32 1
// CHECK:         store float {{.*}} align 4
// CHECK-NOT:     getelementptr inbounds nuw { ptr, i64 }, ptr {{.*}}, i32 0, i32 1
// CHECK:         ret

// A payload the ABI returns indirectly is built straight into storage the
// callee was handed, and what comes back in the aggregate is then the error on
// both paths. The success value has to be read back out of that storage:
// copying element 0 as if it were the payload instead reads the error through a
// payload-typed temp, and reads past it when the payload is wider than the
// error. So there must be no payload temp on the success path at all, and the
// copy into the return slot must come from the storage the callee wrote.
//
// CHECK-LABEL: define {{.*}} @_Z7big_viai(
// CHECK:         call {{.*}} throws_sret(%struct.Big)
// CHECK-NOT:     try.payload
// CHECK:         ret

struct Big { long long a, b, c; };

_Complex float cf(int n) throws {
  _Complex float r;
  __real__ r = n;
  __imag__ r = n + 1;
  return r;
}

Big big_inline(int n) throws { return Big{n, n + 1, n + 2}; }
Big big_via(int n) throws { return big_inline(n); }
