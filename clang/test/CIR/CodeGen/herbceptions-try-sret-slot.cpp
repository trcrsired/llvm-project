// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm %s -o - | FileCheck %s

// A `throws` function whose payload outgrows the register budget returns
// {Str, i8} directly in CIR; the try() call inside reads the payload back
// through the union slot without overflowing the error-sized portion.

namespace std {
struct error_domain_singleton {};
struct error {
  void *d;
  __SIZE_TYPE__ c;
};
}

struct Str { char *b, *c, *e; };

void callee(int) throws;
void g(int) throws;
int intcall(int) throws;

// CHECK-LABEL: define dso_local { %struct.Str, i8 } @_Z1fDri(i32 noundef %0)
// CHECK: call { { ptr, i64 }, i8 } @_Z6calleeDri(i32
// CHECK: ret { %struct.Str, i8 }
Str f(int n) throws {
  Str s{};
  s.b = (char *)(__UINTPTR_TYPE__)n;
  callee(n);
  return s;
}

// CHECK-LABEL: define dso_local { %struct.Str, i8 } @_Z1hDri(i32 noundef %0)
// CHECK: call { { ptr, i64 }, i8 } @_Z7intcallDri(i32
// CHECK: ret { %struct.Str, i8 }
Str h(int n) throws {
  Str s{};
  s.c = (char *)(__UINTPTR_TYPE__)(n + 1);
  int v = try(intcall(n));
  s.e = (char *)(__UINTPTR_TYPE__)v;
  return s;
}

// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z3fwdDri(i32 noundef %0)
// CHECK: call { { ptr, i64 }, i8 } @_Z1gDri(i32
// CHECK: ret { { ptr, i64 }, i8 }
void fwd(int n) throws { g(n); }

// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z2siDri(i32 noundef %0)
// CHECK: ret { { ptr, i64 }, i8 }
int si(int n) throws {
  g(n);
  return 7;
}
