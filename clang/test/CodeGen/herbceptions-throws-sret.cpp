// RUN: %clang -std=c++20 -fherbceptions -fno-exceptions -S -emit-llvm -o - %s | FileCheck %s

// Herbceptions (throws): a payload that the ABI would return indirectly must not
// be bitwise transported through registers -- that detaches the object from its
// own storage, which breaks copy elision and any location-dependent object. Such
// a payload is instead constructed straight into caller storage through a
// 'throws_sret' pointer, and only the error value plus the discriminant are
// returned. The pointer is 'throws_sret' rather than 'sret' because the function
// still returns {error, i1}: 'sret' would require a void return.
//
// A payload whose union with the error fits the register budget keeps the
// previous behaviour and comes back in registers, so small trivially-copyable
// returns are unaffected.
//
// A payload that is smaller than the error is the interesting case. The slot
// has to be sized for max(T, E) so the error fits, but the payload has to be
// constructed through its own type inside it: addressing it as the union would
// put its fields at the union's offsets, and the caller reads the payload back
// at its own. TwoInts has a field at offset 4 where {ptr, i64} has none, so
// building it as the union silently returns 0 for b; ThreeInts is 12 bytes and
// has a third field where the union has only two, which does not even
// type-check. Both are checked so neither can regress.

struct Small { long long a, b; };      // 16 bytes: fits the register budget
struct Big { long long a, b, c, d; };  // 32 bytes: ABI-indirect
struct TwoInts { int a, b; };          // 8 bytes: smaller than the error
struct ThreeInts { int a, b, c; };     // 12 bytes: smaller than the error

// Registers: {Small, i1}, with no hidden pointer argument.
// CHECK-LABEL: define dso_local { %struct.Small, i1 } @_Z5smalli(i32 noundef %0)
// CHECK-NOT:     throws_sret
// CHECK:         ret { %struct.Small, i1 }

// Caller storage: a 'throws_sret' pointer carries the payload, and what comes
// back is {error, i1} -- the fabricated std::error is {ptr, i64}.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z3bigi(ptr noalias writable throws_sret(%struct.Big) align 8 %0, i32 noundef %1)
// CHECK:         ret { { ptr, i64 }, i1 }

// Smaller than the error: the slot is still the union, because that is what
// sizes it for the error, but every payload field is addressed through
// %struct.TwoInts. Addressing them through the union instead is the bug this
// guards: field 1 would land at offset 8 rather than 4 and read back as 0.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z3twoi(i32 noundef %0)
// CHECK:         alloca { ptr, i64 }, align 8
// CHECK-NOT:     getelementptr inbounds nuw { ptr, i64 }
// CHECK:         getelementptr inbounds nuw %struct.TwoInts, ptr %{{[0-9]+}}, i32 0, i32 0
// CHECK:         getelementptr inbounds nuw %struct.TwoInts, ptr %{{[0-9]+}}, i32 0, i32 1
// CHECK-NOT:     getelementptr inbounds nuw { ptr, i64 }
// CHECK:         ret { { ptr, i64 }, i1 }

// 12 bytes: the union has two fields where ThreeInts has three, so addressing
// the payload as the union indexed past its last field and crashed the backend
// rather than merely miscompiling.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z5threei(i32 noundef %0)
// CHECK:         alloca { ptr, i64 }, align 8
// CHECK:         getelementptr inbounds nuw %struct.ThreeInts, ptr %{{[0-9]+}}, i32 0, i32 0
// CHECK:         getelementptr inbounds nuw %struct.ThreeInts, ptr %{{[0-9]+}}, i32 0, i32 1
// CHECK:         getelementptr inbounds nuw %struct.ThreeInts, ptr %{{[0-9]+}}, i32 0, i32 2
// CHECK-NOT:     getelementptr inbounds nuw { ptr, i64 }
// CHECK:         ret { { ptr, i64 }, i1 }

Small small(int n) throws { return Small{n, n}; }
Big big(int n) throws { return Big{n, n, n, n}; }
TwoInts two(int n) throws { return TwoInts{n, n + 1}; }
ThreeInts three(int n) throws { return ThreeInts{n, n + 1, n + 2}; }
