// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fherbceptions -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fherbceptions -fclangir -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s -check-prefix=LLVM

struct value {
  int member;
};

value global{41};

value &source_lvalue() throws { return global; }
value &&source_xvalue() throws { return static_cast<value &&>(global); }

// CIR-LABEL: cir.func{{.*}}@_Z16automatic_lvaluev
// CIR: cir.call @_Z13source_lvaluev
// CIR: cir.if
// CIR: cir.cast bitcast
// CIR: cir.load
// LLVM-LABEL: define{{.*}} { { ptr, i64 }, i1 } @_Z16automatic_lvaluev()
// LLVM: call { { ptr, i64 }, i1 } @_Z13source_lvaluev()
// LLVM: br i1
// LLVM: load ptr, ptr
// LLVM-NOT: load i32
// LLVM: ret { { ptr, i64 }, i1 }
decltype(auto) automatic_lvalue() throws { return source_lvalue(); }

// CIR-LABEL: cir.func{{.*}}@_Z16automatic_xvaluev
// CIR: cir.call @_Z13source_xvaluev
// CIR: cir.if
// CIR: cir.cast bitcast
// CIR: cir.load
// LLVM-LABEL: define{{.*}} { { ptr, i64 }, i1 } @_Z16automatic_xvaluev()
// LLVM: call { { ptr, i64 }, i1 } @_Z13source_xvaluev()
// LLVM: br i1
// LLVM: load ptr, ptr
// LLVM-NOT: load i32
// LLVM: ret { { ptr, i64 }, i1 }
decltype(auto) automatic_xvalue() throws { return source_xvalue(); }

// A source-level pointer contract belongs only to the active success payload.
// It must not be attached to the shaped CIR/LLVM return value.
// CIR-LABEL: cir.func{{.*}}@_Z14nonnull_resultv
// CIR-NOT: llvm.nonnull
// LLVM-LABEL: define{{.*}} { { ptr, i64 }, i1 } @_Z14nonnull_resultv()
__attribute__((returns_nonnull)) value *nonnull_result() throws {
  return &global;
}
