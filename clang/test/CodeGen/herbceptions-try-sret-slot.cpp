// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fherbceptions -emit-llvm -discard-value-names %s -o %t.x86.ll
// RUN: FileCheck --input-file=%t.x86.ll --check-prefixes=CHECK,X86 %s
// RUN: %clang_cc1 -std=c++20 -triple aarch64-unknown-linux-gnu -fherbceptions -emit-llvm -discard-value-names %s -o %t.a64.ll
// RUN: FileCheck --input-file=%t.a64.ll --check-prefixes=CHECK,X86 %s
// RUN: %clang_cc1 -std=c++20 -triple wasm32-unknown-wasip1 -fherbceptions -emit-llvm -discard-value-names %s -o %t.w32.ll
// RUN: FileCheck --input-file=%t.w32.ll --check-prefixes=CHECK,W32 %s

// try() mirrors the callee's error payload into the enclosing function's
// return slot on the success path as well as the error path, so the slot is
// defined on both. That is only valid while the slot is pure error storage:
// in a non-void throws function the slot is the union{T,E} that may already
// hold the live return object -- an indirect payload is constructed directly
// in the throws_sret storage, and NRVO aliases a local into it. Writing the
// error-sized value there then corrupts the object's fields. The mirror must
// only be emitted for void throws functions, whose slot is always error-only.

namespace std {
struct error_domain_singleton {};
struct error {
  void *d;
  __SIZE_TYPE__ c;
};
}

struct Str { char *b, *c, *e; }; // 24 bytes: throws_sret on every target

void callee(int) throws;
void g(int) throws;
int intcall(int) throws;

// s is NRVO'd into the throws_sret slot %0. The success arm of the try'd
// callee() call must leave %0 alone -- the payload %11 it would store is the
// callee's error, garbage on this path.
//
// CHECK-LABEL: define {{.*}} @_Z1fi(
// X86:        call { { ptr, i64 }, i1 } @_Z6calleei
// W32:        call i1 @_Z6calleei
// CHECK:      br i1 {{.*}}, label %[[ERR:[0-9]+]], label %[[OK:[0-9]+]]
// CHECK:    [[ERR]]:
// X86:        store { ptr, i64 } {{.*}}, ptr %0
// W32:        store { ptr, i32 } {{.*}}, ptr %0
// CHECK:      store i1 true
// CHECK:    [[OK]]:
// CHECK-NOT:  store { ptr, {{.*}} } {{.*}}, ptr %0
// CHECK-NOT:  memset{{.*}}%0
// CHECK:      br label
Str f(int n) throws {
  Str s{};
  s.b = (char *)(__UINTPTR_TYPE__)n;
  callee(n);
  return s;
}

// Same hazard through a non-void try(): the try value is materialised below
// the mirror point, so %0 must stay untouched on the success arm.
//
// CHECK-LABEL: define {{.*}} @_Z1hi(
// X86:        call { { ptr, i64 }, i1 } @_Z7intcalli
// W32:        call i1 @_Z7intcalli
// CHECK:      br i1 {{.*}}, label %[[ERR2:[0-9]+]], label %[[OK2:[0-9]+]]
// CHECK:    [[ERR2]]:
// CHECK:      store i1 true
// CHECK:    [[OK2]]:
// CHECK-NOT:  store { ptr, {{.*}} } {{.*}}, ptr %0
// CHECK-NOT:  memset{{.*}}%0
// CHECK:      br label
Str h(int n) throws {
  Str s{};
  s.c = (char *)(__UINTPTR_TYPE__)(n + 1);
  int v = try(intcall(n));
  s.e = (char *)(__UINTPTR_TYPE__)v;
  return s;
}

// A void throws function's return slot is error-only, so the mirror store is
// still emitted there -- without it the slot is undef on the success arm and
// SROA forms a select with an undef arm that blocks tail calls.
//
// CHECK-LABEL: define {{.*}} @_Z3fwdi(
// X86:        call { { ptr, i64 }, i1 } @_Z1gi
// W32:        call i1 @_Z1gi
// CHECK:      br i1 {{.*}}, label %[[ERR3:[0-9]+]], label %[[OK3:[0-9]+]]
// CHECK:    [[OK3]]:
// X86-NEXT:   store { ptr, i64 }
// W32-NEXT:   store { ptr, i32 }
// CHECK-NEXT: br label
void fwd(int n) throws { g(n); }

// A scalar throws return's slot is a dead temp until the return writes it, so
// the mirror is simply skipped; propagation still works.
//
// CHECK-LABEL: define {{.*}} @_Z2sii(
// X86:        call { { ptr, i64 }, i1 } @_Z1gi
// W32:        call i1 @_Z1gi
// CHECK:      br i1 {{.*}}, label %[[ERR4:[0-9]+]], label %[[OK4:[0-9]+]]
// CHECK:    [[ERR4]]:
// CHECK:      store i1 true
// CHECK:    [[OK4]]:
// X86:        store i64 7
// W32:        store i32 7
// CHECK:      br label
int si(int n) throws {
  g(n);
  return 7;
}
