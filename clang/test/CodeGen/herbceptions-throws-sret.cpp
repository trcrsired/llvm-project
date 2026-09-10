// RUN: %clang -std=c++20 -fherbceptions -fno-exceptions -S -emit-llvm -o - %s | FileCheck %s

// Herbception (throws): a payload that the ABI would return indirectly must not
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

struct Small { long long a, b; };      // 16 bytes: fits the register budget
struct Big { long long a, b, c, d; };  // 32 bytes: ABI-indirect

// Registers: {Small, i1}, with no hidden pointer argument.
// CHECK-LABEL: define dso_local { %struct.Small, i1 } @_Z5smalli(i32 noundef %0)
// CHECK-NOT:     throws_sret
// CHECK:         ret { %struct.Small, i1 }

// Caller storage: a 'throws_sret' pointer carries the payload, and what comes
// back is {error, i1} -- the fabricated std::error is {ptr, i64}.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z3bigi(ptr noalias writable throws_sret(%struct.Big) align 8 %0, i32 noundef %1)
// CHECK:         ret { { ptr, i64 }, i1 }

Small small(int n) throws { return Small{n, n}; }
Big big(int n) throws { return Big{n, n, n, n}; }
