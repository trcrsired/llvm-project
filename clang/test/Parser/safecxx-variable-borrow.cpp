// RUN: %clang_cc1 -fsyntax-only -fsafecxx -ast-dump %s | FileCheck %s
// RUN: %clang_cc1 -fsyntax-only -fparameter-passing %s
//
// D4: `in` / `inout` in variable decl-specifier position. They lower to
// tracked references (const T& / T&), even for cheap-to-copy element types,
// so the borrow and its provenance are preserved. Works with or without the
// -fsafecxx umbrella, keyed on -fparameter-passing.

static void sink(in int) noexcept;

[[unsafe]] {
struct V {
  int a[4];
  int *begin() noexcept { return a; }
  int *end() noexcept { return a + 4; }
  const int *begin() const noexcept { return a; }
  const int *end() const noexcept { return a + 4; }
  int &operator[](int i) noexcept [[clang::lifetimebound]] { return a[i]; }
  const int &operator[](int i) const noexcept
      [[clang::lifetimebound]] { return a[i]; }
};
}

// CHECK: FunctionDecl{{.*}} lower
// CHECK-NEXT: ParmVarDecl{{.*}} v 'V &'
// CHECK: VarDecl{{.*}} b 'const int &'
// CHECK: VarDecl{{.*}} c 'int &'
// CHECK: VarDecl{{.*}} d 'int'
static void lower(inout V v) noexcept {
  in auto b = v[1];    // read-only tracked borrow
  inout auto c = v[2]; // mutable tracked borrow
  auto d = v[3];       // plain copy
  sink(b);
  c = 1;
  sink(d);
}

// Explicit type works too and is still lowered to a reference.
// CHECK: VarDecl{{.*}} e 'const int &'
// CHECK: VarDecl{{.*}} f 'int &'
static void explicit_type(in int x, inout int y) noexcept {
  in int e = x;
  inout int f = y;
  f = e;
}

// Cheap-to-copy element types are NOT copied: the borrow must survive.
struct Small { int x; };
static Small getSmall() noexcept { return Small{1}; }
// CHECK: VarDecl{{.*}} s 'const Small &'
static void cheap() noexcept {
  in auto s = getSmall();
  sink(s.x);
}

// Range-for loop variables use the same spelling and lowering.
// CHECK: VarDecl{{.*}} x 'const int &'
// CHECK: VarDecl{{.*}} y 'int &'
static void loops(inout V v) noexcept {
  for (in auto x : v)
    sink(x);
  for (inout auto y : v)
    y = 1;
}

// The keywords are contextual: an ordinary variable named `in` still works.
static void contextual() noexcept {
  int in = 1;
  in = 2;
  sink(in);
}
