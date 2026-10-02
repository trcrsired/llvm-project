// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm %s -o - | FileCheck %s

// Herbceptions (throws): the return slot is a union, sized for max(T, E) so
// it can hold either the success value or the error. Anything that constructs
// or reads the payload goes through the payload's own type.

namespace std {
struct error { void *d; __SIZE_TYPE__ c; };
}

// A _Complex float payload (8 bytes) is smaller than the error (16 bytes) so
// the union slot is {ptr, i64}, and the payload is addressed through its own
// {float, float} type -- the imag component belongs at offset 4, not at the
// union's second field.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z2cfDri(i32 noundef %0)
// CHECK: getelementptr inbounds nuw { float, float }, ptr %{{[0-9]+}}, i32 0, i32 0
// CHECK: getelementptr inbounds nuw { float, float }, ptr %{{[0-9]+}}, i32 0, i32 1
// CHECK: ret { { ptr, i64 }, i8 }
_Complex float cf(int n) throws {
  _Complex float r;
  __real__ r = n;
  __imag__ r = n + 1;
  return r;
}

struct Big { long long a, b, c; };

// A 24-byte payload outgrows the error and is returned as {Big, i8}.
// CHECK-LABEL: define dso_local { %struct.Big, i8 } @_Z10big_inlineDri(i32 noundef %0)
// CHECK: ret { %struct.Big, i8 }
Big big_inline(int n) throws { return Big{n, n + 1, n + 2}; }

// CHECK-LABEL: define dso_local { %struct.Big, i8 } @_Z7big_viaDri(i32 noundef %0)
// CHECK: call { %struct.Big, i8 } @_Z10big_inlineDri(i32
// CHECK: ret { %struct.Big, i8 }
Big big_via(int n) throws { return big_inline(n); }
