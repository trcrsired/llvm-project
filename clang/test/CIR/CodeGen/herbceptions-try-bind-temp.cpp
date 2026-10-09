// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm -discard-value-names -o - %s | FileCheck %s --check-prefix=CIR

// A bare call to a `throws` function whose result is a class type requiring
// destruction wraps the CallExpr in a CXXBindTemporaryExpr. emitHerbceptionTry
// must strip it (IgnoreImplicit) — IgnoreParenImpCasts stops at the bind
// temporary and the unchecked cast<CallExpr> that follows crashed emitCallee.
struct S {
  int v;
  ~S() {}
};

S make_s() throws { return {1}; }

// CIR-LABEL: define {{.*}} @_Z4takeDrv
// CIR:         call {{.*}} @_Z6make_sDrv
// CIR:         br i1
void take() throws {
  make_s();
}

// The same shape through emitHerbceptionCatchReturnFailure's strip point:
// an auto-propagated return_failure call whose result needs destruction
// also carries a bind-temporary wrapper on the subexpr.
struct T {
  int v;
  ~T() {}
};
T make_t() return_failure{int} { return {2}; }

// CIR-LABEL: define {{.*}} @_Z5take2DEiEv
// CIR:         call {{.*}} @_Z6make_tDEiEv
// CIR:         br i1
void take2() return_failure{int} {
  make_t();
}
