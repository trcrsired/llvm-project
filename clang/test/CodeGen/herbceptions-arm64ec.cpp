// RUN: %clang_cc1 -triple arm64ec-pc-windows-msvc -std=c++26 -fherbceptions -fms-extensions -fexceptions -emit-llvm -o - %s | FileCheck %s

// Herbceptions on arm64ec: throws functions return {T, i1} where the payload
// position is the union of T and the error. The ARM64EC return convention
// (RetCC_AArch64_Arm64EC_Throws) keeps a 16-byte payload in x0:x1 -- matching
// std::span/std::string_view -- with the discriminant in NZCV.C, so the
// frontend must emit direct returns and register-sized arguments for these
// types rather than the indirect forms the stock Windows ABI would use.

struct Empty {};
struct Span16 { void *p; unsigned long n; };      // 16B trivial: std::span-like
struct Big32 { long a, b, c, d; };                // 32B trivial

Span16 ret_span() throws;
Empty ret_empty() throws;
Big32 ret_big() throws;
Span16 takes_args(Span16 s, Empty e, int x) throws;

// The caller sees the discriminated shapes at the call site. The empty-struct
// argument is dropped from the signature entirely: a throws function's empty
// argument consumes no register slot on ARM64EC.
// CHECK: call { %struct.Span16, i1 } @"?ret_span@@YA?AUSpan16@@XZ"()
// CHECK: call { %struct.Span16, i1 } @"?takes_args@@YA?AUSpan16@@U1@UEmpty@@H@Z"([2 x i64] {{.*}}, i32 noundef 7)
// CHECK-NOT: takes_args@@YA?AUSpan16@@U1@UEmpty@@H@Z"([2 x i64] {{.*}}, i64
// CHECK: call { { ptr, i64 }, i1 } @"?ret_empty@@YA?AUEmpty@@XZ"()
// CHECK: call { %struct.Big32, i1 } @"?ret_big@@YA?AUBig32@@XZ"()

void caller() throws {
  Empty e;
  Span16 s = ret_span();
  Span16 r = takes_args(s, e, 7);
  Empty e2 = ret_empty();
  Big32 b = ret_big();
  (void)r;
  (void)e2;
  (void)b;
}

// A 16-byte payload stays a direct {T, i1} return, never sret.
// CHECK: declare dso_local { %struct.Span16, i1 } @"?ret_span@@YA?AUSpan16@@XZ"()

// Arguments of a throws function are passed in registers under the EC
// convention: the 16-byte aggregate is coerced to [2 x i64] (x0:x1) and the
// empty struct is ignored -- it does not consume an argument register slot.
// CHECK: declare dso_local { %struct.Span16, i1 } @"?takes_args@@YA?AUSpan16@@U1@UEmpty@@H@Z"([2 x i64], i32 noundef)

// An empty payload still returns the error union {ptr, i64} plus the
// discriminant: the union slot must carry the error on failure, so no
// register is spent on the empty struct itself.
// CHECK: declare dso_local { { ptr, i64 }, i1 } @"?ret_empty@@YA?AUEmpty@@XZ"()

// A 32-byte trivial payload also returns directly as {T, i1}; the ARM64EC
// throws convention covers it with x0:x3, like plain AArch64.
// CHECK: declare dso_local { %struct.Big32, i1 } @"?ret_big@@YA?AUBig32@@XZ"()
