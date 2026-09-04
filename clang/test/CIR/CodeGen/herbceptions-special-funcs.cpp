// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fherbceptions -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fherbceptions -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s -check-prefix=LLVM
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fherbceptions -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s -check-prefix=OGCG

// Active constructors use the Herbception return channel even though their
// source-level result is void. Check both the complete (C1) and base (C2)
// variants: declarations/definitions and constructor-call arrangement use
// separate CodeGen paths and must agree on the physical ABI. A conditional
// throws(false) constructor remains ABI-identical to an ordinary constructor.

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
};
} // namespace std

struct Active {
  int value;
  Active(int value) throws : value(value) {}
  Active(const Active &other) throws : value(other.value) {}
};

struct Disabled {
  int value;
  Disabled(int value) throws(false) : value(value) {}
  Disabled(const Disabled &other) throws(false) : value(other.value) {}
};

// The textual CIR signatures directly guard the two structor-declaration
// variants. The C1-to-C2 calls additionally guard constructor-call
// arrangement. Active calls return an anonymous {std::error, bool} record;
// disabled calls keep CIR's ordinary void result.
//
// CIR-DAG: cir.func{{.*}} @_ZN6ActiveC1Ei({{.*}}) -> !cir.struct<{{.*}}> {{.*}}cir.throws
// CIR-DAG: cir.func{{.*}} @_ZN6ActiveC1ERKS_({{.*}}) -> !cir.struct<{{.*}}> {{.*}}cir.throws
// CIR-DAG: cir.func{{.*}} @_ZN6ActiveC2Ei({{.*}}) -> !cir.struct<{{.*}}> {{.*}}cir.throws
// CIR-DAG: cir.func{{.*}} @_ZN6ActiveC2ERKS_({{.*}}) -> !cir.struct<{{.*}}> {{.*}}cir.throws
// CIR-DAG: cir.call @_ZN6ActiveC2Ei({{.*}}){{.*}} -> !cir.struct<
// CIR-DAG: cir.call @_ZN6ActiveC2ERKS_({{.*}}){{.*}} -> !cir.struct<
//
// CIR-DAG: cir.func{{.*}} @_ZN8DisabledC1Ei({{.*}}) func_info<
// CIR-DAG: cir.func{{.*}} @_ZN8DisabledC1ERKS_({{.*}}) func_info<
// CIR-DAG: cir.func{{.*}} @_ZN8DisabledC2Ei({{.*}}) func_info<
// CIR-DAG: cir.func{{.*}} @_ZN8DisabledC2ERKS_({{.*}}) func_info<
// CIR-DAG: cir.call @_ZN8DisabledC2Ei({{.*}}){{.*}} -> ()
// CIR-DAG: cir.call @_ZN8DisabledC2ERKS_({{.*}}){{.*}} -> ()

// CIR-to-LLVM and classic CodeGen must agree on both sides of every call.
// LLVM-LABEL: define dso_local i32 @main()
// LLVM: call { { ptr, i64 }, i1 } @_ZN6ActiveC1Ei
// LLVM: call { { ptr, i64 }, i1 } @_ZN6ActiveC1ERKS_
// LLVM: call void @_ZN8DisabledC1Ei
// LLVM: call void @_ZN8DisabledC1ERKS_
//
// LLVM-LABEL: define linkonce_odr { { ptr, i64 }, i1 } @_ZN6ActiveC1Ei(
// LLVM: call { { ptr, i64 }, i1 } @_ZN6ActiveC2Ei
// LLVM-LABEL: define linkonce_odr { { ptr, i64 }, i1 } @_ZN6ActiveC1ERKS_(
// LLVM: call { { ptr, i64 }, i1 } @_ZN6ActiveC2ERKS_
// LLVM-LABEL: define linkonce_odr void @_ZN8DisabledC1Ei(
// LLVM: call void @_ZN8DisabledC2Ei
// LLVM-LABEL: define linkonce_odr void @_ZN8DisabledC1ERKS_(
// LLVM: call void @_ZN8DisabledC2ERKS_
//
// LLVM-LABEL: define linkonce_odr { { ptr, i64 }, i1 } @_ZN6ActiveC2Ei(
// LLVM-LABEL: define linkonce_odr { { ptr, i64 }, i1 } @_ZN6ActiveC2ERKS_(
// LLVM-LABEL: define linkonce_odr void @_ZN8DisabledC2Ei(
// LLVM-LABEL: define linkonce_odr void @_ZN8DisabledC2ERKS_(

// OGCG-LABEL: define dso_local noundef i32 @main()
// OGCG: call { { ptr, i64 }, i1 } @_ZN6ActiveC1Ei
// OGCG: call { { ptr, i64 }, i1 } @_ZN6ActiveC1ERKS_
// OGCG: call void @_ZN8DisabledC1Ei
// OGCG: call void @_ZN8DisabledC1ERKS_
//
// OGCG-LABEL: define linkonce_odr { { ptr, i64 }, i1 } @_ZN6ActiveC1Ei(
// OGCG: call { { ptr, i64 }, i1 } @_ZN6ActiveC2Ei
// OGCG-LABEL: define linkonce_odr { { ptr, i64 }, i1 } @_ZN6ActiveC1ERKS_(
// OGCG: call { { ptr, i64 }, i1 } @_ZN6ActiveC2ERKS_
// OGCG-LABEL: define linkonce_odr void @_ZN8DisabledC1Ei(
// OGCG: call void @_ZN8DisabledC2Ei
// OGCG-LABEL: define linkonce_odr void @_ZN8DisabledC1ERKS_(
// OGCG: call void @_ZN8DisabledC2ERKS_
//
// OGCG-LABEL: define linkonce_odr { { ptr, i64 }, i1 } @_ZN6ActiveC2Ei(
// OGCG-LABEL: define linkonce_odr { { ptr, i64 }, i1 } @_ZN6ActiveC2ERKS_(
// OGCG-LABEL: define linkonce_odr void @_ZN8DisabledC2Ei(
// OGCG-LABEL: define linkonce_odr void @_ZN8DisabledC2ERKS_(

int main() {
  Active active(10);
  Active activeCopy = active;
  Disabled disabled(20);
  Disabled disabledCopy = disabled;
  return 0;
}
