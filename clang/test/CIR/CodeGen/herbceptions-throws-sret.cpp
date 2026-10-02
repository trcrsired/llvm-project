// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm %s -o - | FileCheck %s

// Herbceptions (throws): CIR keeps the shaped {payload, i8} wire form for
// throws functions rather than routing a large payload through a hidden
// 'throws_sret' pointer. The payload member is still constructed through its
// own type inside the union slot, so a payload smaller than the error is
// addressed field-by-field as its declared type.

struct Small { long long a, b; };      // 16 bytes
struct Big { long long a, b, c, d; };  // 32 bytes
struct TwoInts { int a, b; };          // 8 bytes: smaller than the error
struct ThreeInts { int a, b, c; };     // 12 bytes: smaller than the error

// CHECK-LABEL: define dso_local { %struct.Small, i8 } @_Z5smallDri(i32 noundef %0)
// CHECK: ret { %struct.Small, i8 }
Small small(int n) throws {
  return {n, n + 1};
}

// CIR lowers the 32-byte payload as a direct shaped return.
// CHECK-LABEL: define dso_local { %struct.Big, i8 } @_Z3bigDri(i32 noundef %0)
// CHECK: ret { %struct.Big, i8 }
Big big(int n) throws {
  return {n, n + 1, n + 2, n + 3};
}

// Smaller than the error: the slot is sized for {ptr, i64}, but every payload
// field is addressed through %struct.TwoInts.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z3twoDri(i32 noundef %0)
// CHECK: getelementptr inbounds nuw %struct.TwoInts, ptr %{{[0-9]+}}, i32 0, i32 0
// CHECK: getelementptr inbounds nuw %struct.TwoInts, ptr %{{[0-9]+}}, i32 0, i32 1
// CHECK: ret { { ptr, i64 }, i8 }
TwoInts two(int n) throws {
  return {n, n + 1};
}

// 12 bytes: the union has two fields where ThreeInts has three.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z5threeDri(i32 noundef %0)
// CHECK: getelementptr inbounds nuw %struct.ThreeInts, ptr %{{[0-9]+}}, i32 0, i32 0
// CHECK: getelementptr inbounds nuw %struct.ThreeInts, ptr %{{[0-9]+}}, i32 0, i32 1
// CHECK: getelementptr inbounds nuw %struct.ThreeInts, ptr %{{[0-9]+}}, i32 0, i32 2
// CHECK: ret { { ptr, i64 }, i8 }
ThreeInts three(int n) throws {
  return {n, n + 1, n + 2};
}
