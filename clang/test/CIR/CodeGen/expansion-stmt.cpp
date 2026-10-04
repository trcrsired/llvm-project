// RUN: %clang_cc1 -std=c++26 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR
// RUN: %clang_cc1 -std=c++26 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s -check-prefix=LLVM

void h(int, int);

void simple() {
  template for (auto x : {1, 2}) {
    h(1, x);
  }
}

// CIR-LABEL: cir.func {{.*}} @_Z6simplev()
// CIR:         cir.scope {
// CIR:           cir.scope {
// CIR:             cir.alloca "x"
// CIR:             cir.call @_Z1hii
// CIR:           cir.label "cir.expand.0.cont.1"
// CIR:           cir.scope {
// CIR:             cir.call @_Z1hii
// CIR:           cir.label "cir.expand.0.end"

// LLVM-LABEL: define dso_local void @_Z6simplev()
// LLVM:         call void @_Z1hii(i32 noundef 1, i32 noundef %{{.*}})
// LLVM:         call void @_Z1hii(i32 noundef 1, i32 noundef %{{.*}})
// LLVM:         ret void

void break_continue() {
  template for (auto x : {1, 2}) {
    break;
    h(1, x);
  }

  template for (auto x : {3, 4}) {
    continue;
    h(2, x);
  }

  template for (auto x : {5, 6}) {
    if (x == 2) break;
    h(3, x);
  }

  template for (auto x : {7, 8}) {
    if (x == 2) continue;
    h(4, x);
  }
}

// CIR-LABEL: cir.func {{.*}} @_Z14break_continuev()
// 'break' exits the entire expansion.
// CIR:           cir.goto "cir.expand.0.end"
// CIR:           cir.label "cir.expand.0.cont.1"
// CIR:           cir.goto "cir.expand.0.end"
// CIR:           cir.label "cir.expand.0.end"
// 'continue' proceeds to the next instantiation.
// CIR:           cir.goto "cir.expand.1.cont.1"
// CIR:           cir.label "cir.expand.1.cont.1"
// CIR:           cir.goto "cir.expand.1.end"
// CIR:           cir.label "cir.expand.1.end"
// 'break'/'continue' inside nested control flow still reach the labels.
// CIR:           cir.if {{.*}} {
// CIR:             cir.goto "cir.expand.2.end"
// CIR:           cir.if {{.*}} {
// CIR:             cir.goto "cir.expand.2.end"
// CIR:           cir.label "cir.expand.2.end"
// CIR:           cir.if {{.*}} {
// CIR:             cir.goto "cir.expand.3.cont.1"
// CIR:           cir.if {{.*}} {
// CIR:             cir.goto "cir.expand.3.end"
// CIR:           cir.label "cir.expand.3.end"

// LLVM-LABEL: define dso_local void @_Z14break_continuev()
// The first two expansions exit before doing anything, so only the last two
// emit calls.
// LLVM:         call void @_Z1hii(i32 noundef 3, i32 noundef %{{.*}})
// LLVM:         call void @_Z1hii(i32 noundef 3, i32 noundef %{{.*}})
// LLVM:         call void @_Z1hii(i32 noundef 4, i32 noundef %{{.*}})
// LLVM:         call void @_Z1hii(i32 noundef 4, i32 noundef %{{.*}})
// LLVM:         ret void

int break_continue_nested() {
  int sum = 0;

  template for (auto x : {1, 2}) {
    template for (auto y : {3, 4}) {
      if (x == 2) break;
      sum += y;
    }
    sum += x;
  }

  return sum;
}

// CIR-LABEL: cir.func {{.*}} @_Z21break_continue_nestedv()
// 'break' inside the inner expansion binds to the inner expansion.
// CIR:           cir.if {{.*}} {
// CIR:             cir.goto "cir.expand.1.end"
// CIR:           cir.label "cir.expand.1.cont.1"
// CIR:           cir.if {{.*}} {
// CIR:             cir.goto "cir.expand.1.end"
// CIR:           cir.label "cir.expand.1.end"
// CIR:           cir.label "cir.expand.0.end"

// LLVM-LABEL: define dso_local noundef i32 @_Z21break_continue_nestedv()
// LLVM:         ret i32

struct S {
  int a, b;
};

int iterating() {
  int sum = 0;
  template for (auto x : S{1, 2})
    sum += x;
  return sum;
}

// CIR-LABEL: cir.func {{.*}} @_Z9iteratingv()
// CIR:         cir.scope {
// CIR:           cir.scope {
// CIR:             cir.alloca
// CIR:           cir.label "cir.expand.0.cont.1"
// CIR:           cir.scope {
// CIR:             cir.alloca
// CIR:           cir.label "cir.expand.0.end"

// LLVM-LABEL: define dso_local noundef i32 @_Z9iteratingv()
// LLVM:         ret i32

void loop_inside(int n) {
  // 'break'/'continue' in a loop nested in an expansion still bind to the
  // loop, not the expansion.
  template for (auto x : {1, 2}) {
    for (int i = 0; i < n; ++i) {
      if (i == x) continue;
      if (i > 3) break;
      h(i, x);
    }
  }
}

// CIR-LABEL: cir.func {{.*}} @_Z11loop_insidei
// CIR:           cir.for : cond {
// CIR:           } body {
// CIR:             cir.continue
// CIR:             cir.break
// CIR:           } step {
// CIR:           }
// CIR-NOT:       cir.goto "cir.expand.0
// CIR:           cir.label "cir.expand.0.cont.1"
// CIR:           cir.label "cir.expand.0.end"
