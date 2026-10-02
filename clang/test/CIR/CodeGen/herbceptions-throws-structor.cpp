// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fherbceptions -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fherbceptions -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s -check-prefix=LLVM

// A throws constructor carries the shaped {E, i1} return signature like any
// other throws function. A throwing call in a member initializer or in the
// structor body propagates its error through that signature instead of
// crashing or silently dropping it.

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
};
}

int may_fail() throws;
void may_throw() throws;

struct Widget {
  int x;
  // A throwing call in a member initializer of a throws constructor.
  Widget() throws : x(may_fail()) {}
};

struct Other {
  int x;
  // A throwing call in a throws constructor body.
  Other() throws { x = may_fail(); }
};

struct Manual {
  int x;
  // An explicit 'throw throws' inside a throws constructor.
  Manual(int bad) throws {
    if (bad < 0)
      throw throws std::error{nullptr, 1};
    x = bad;
  }
};

// A 'return;' in a void throws function still has to produce the shaped
// {E, i1} result.
// CIR: cir.func {{.*}}@_Z10early_exitDrv() -> !rec_
// CIR-SAME: cir.throws
// LLVM: define {{.*}} { { ptr, i64 }, i8 } @_Z10early_exitDrv(
void early_exit() throws {
  return;
}

// main() traps when a throws error escapes it.
// CIR-LABEL: cir.func {{.*}}@main()
// CIR: cir.trap
// LLVM-LABEL: define {{.*}}@main()
// LLVM: call void @llvm.trap
int main() {
  try {
    Widget w;
    Other o;
    Manual m(1);
    early_exit();
  } catch throws(std::error) {
  }
  may_throw();
  return 0;
}

// The constructor definitions are emitted last (linkonce_odr). Each must
// carry the shaped return signature and the cir.throws attribute.

// CIR: cir.func {{.*}}@_ZN6WidgetC1EDrv({{.*}}) -> !rec_
// CIR-SAME: cir.throws
// LLVM: define {{.*}} { { ptr, i64 }, i8 } @_ZN6WidgetC1EDrv(

// CIR: cir.func {{.*}}@_ZN5OtherC1EDrv({{.*}}) -> !rec_
// CIR-SAME: cir.throws

// CIR: cir.func {{.*}}@_ZN6ManualC1EDri({{.*}}) -> !rec_
// CIR-SAME: cir.throws
