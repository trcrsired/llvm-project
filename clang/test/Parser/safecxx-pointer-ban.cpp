// RUN: %clang_cc1 -fsyntax-only -verify -fsafecxx %s
//
// Rule 1 / D24, revised: pointers are opaque tokens in safe C++. Naming a
// pointer type is allowed (declarations, parameters, fields, typedefs);
// looking through one is rejected at the point of use, in
// safecxx-opaque-pointer.cpp. References and pointer-to-member types remain
// confined to [[unsafe]] fences. The check is on the source type before d0708
// lowering, so in/out/inout/move parameters stay legal.

// --- pointers are allowed in safe source -------------------------------------

static int *ptr_return() noexcept;
static void ptr_param(int *p) noexcept { (void)p; }

struct PtrField {
  int *ptr;
};
typedef int *IntPtr;

static void ptr_local() noexcept {
  int *p = nullptr;
  (void)p;
  int *arr[2];
  (void)arr;
}

// [[unsafe]] on a function is still only a call restriction; its declaration
// is safe source.
[[unsafe]] static int *unsafe_fn() noexcept;

// --- banned: references and pointer-to-member --------------------------------

// expected-error@+1 {{raw pointer or reference type}}
static int &bad_lref_return() noexcept;

// expected-error@+1 {{raw pointer or reference type}}
static void bad_lref_param(int &r) noexcept { (void)r; }

// expected-error@+1 {{raw pointer or reference type}}
static void bad_rref_param(int &&r) noexcept { (void)r; }

struct S {
  int m;
};
// expected-error@+1 {{raw pointer or reference type}}
static int S::*bad_member_ptr = nullptr;

static void bad_local() noexcept {
  int x = 0;
  // expected-error@+1 {{raw pointer or reference type}}
  int &r = x;
  (void)r;
}

// --- allowed inside [[unsafe]] fences ----------------------------------------

static void body_fence() noexcept {
  [[unsafe]] {
    int *p = nullptr;
    (void)p;
    int x = 0;
    int &r = x;
    (void)r;
  }
}

[[unsafe]] {
  static int *fenced_return() noexcept { return nullptr; }
  static void fenced_params(int *p, int &r) noexcept { (void)p; (void)r; }
  struct FencedStruct {
    int *ptr;
  };
  static int *fenced_global;
  typedef int *FencedPtr;
}

static void use_fenced() noexcept {
  (void)fenced_return();
  FencedStruct fs{};
  [[unsafe]] { fenced_params(fs.ptr, *fenced_global); }
}

// --- d0708 parameters are unaffected by the ban ------------------------------

struct Big {
  char buf[64];
};
static void d0708(in int x, out int y, inout int z, move int w) noexcept {
  y = x + z;
  (void)w;
}
static void d0708_big(in Big b) noexcept { (void)b; }

// --- range-for ---------------------------------------------------------------

[[unsafe]] {
struct V {
  int a[4];
  int *begin() noexcept { return a; }
  int *end() noexcept { return a + 4; }
};
}
static void range_value(inout V v) noexcept {
  // expected-error@+1 {{range-for loop variable must declare its passing intent}}
  for (auto x : v) (void)x;
}
static void range_ref(inout V v) noexcept {
  // expected-error@+1 {{raw pointer or reference type}} expected-error@+1 {{range-for loop variable must declare its passing intent}}
  for (auto &x : v) (void)x;
}
