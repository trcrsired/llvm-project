// RUN: %clang -fherbceptions -fno-exceptions -S -emit-llvm -o - %s | FileCheck %s
// RUN: not %clang -S -emit-llvm %s 2>&1 | FileCheck %s --check-prefix=DISABLED

// Herbception (throws): a function declared 'throws' is lowered to a {T, i1}
// return type with the llvm 'throws' attribute. -fherbceptions is required for
// the keyword; it is independent of -fno-exceptions.

// CHECK: define dso_local { i32, i1 } @_Z3fooi(i32 noundef %x) #[[ATTR:[0-9]+]]
// CHECK-NOT: call void @__cxa_throw
// CHECK: ret { i32, i1 }
int foo(int x) throws {
  if (x < 0) throw throws 42;
  return x + 1;
}

// Plain functions (no throws) are unchanged.
// CHECK-LABEL: define dso_local noundef i32 @_Z4plinii(i32 noundef %a, i32 noundef %b)
// CHECK-NEXT: entry:
int plin(int a, int b) { return a + b; }

// try(expr) auto-propagates the error of a throws call. The caller extracts
// the discriminant, branches on it, and on error returns {err, true}.
// CHECK-LABEL: define dso_local { i32, i1 } @_Z6calleri(i32 noundef %x)
// CHECK:         call { i32, i1 } @_Z3fooi
// CHECK:         extractvalue { i32, i1 } %{{.*}}, 1
// CHECK:         br i1 %{{.*}}, label %try.err, label %try.ok
int caller(int x) throws {
  return try(foo(x));
}

// CHECK: attributes #[[ATTR]] = { {{.*}}throws{{.*}} }

// Without -fherbceptions, 'throws' is not a keyword.
// DISABLED: error: expected function body after function declarator
