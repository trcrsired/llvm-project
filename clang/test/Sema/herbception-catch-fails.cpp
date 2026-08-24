// RUN: not %clang_cc1 -fherbceptions -fsyntax-only %s 2>&1 | FileCheck %s

int plain(int x) { return x; }
int bar(int x) fails{int} { if (x < 0) return failure(x); return x + 1; }

// CHECK: error: 'catch fails' expression operand must be a call to a function declared 'throws' or 'fails{...}'
int f1() {
  auto e = catch fails(plain(1));
  return 0;
}
