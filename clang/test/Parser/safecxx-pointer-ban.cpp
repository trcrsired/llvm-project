// RUN: %clang_cc1 -fsyntax-only -verify -fsafecxx %s
//
// Rule 1 / D24: naming a raw pointer, reference, or pointer-to-member type in
// a declaration is confined to [[unsafe]] fences. The check is on the source
// type before d0708 lowering, so in/out/inout/move parameters stay legal, and
// transient references produced by expressions are not declarations.

// --- banned in safe source ---------------------------------------------------

// expected-error@+1 {{raw pointer or reference type}}
static int *bad_return() noexcept;

// expected-error@+1 {{raw pointer or reference type}}
static int &bad_lref_return() noexcept;

// expected-error@+1 {{raw pointer or reference type}}
static void bad_ptr_param(int *p) noexcept { (void)p; }

// expected-error@+1 {{raw pointer or reference type}}
static void bad_lref_param(int &r) noexcept { (void)r; }

// expected-error@+1 {{raw pointer or reference type}}
static void bad_rref_param(int &&r) noexcept { (void)r; }

struct BadField {
  // expected-error@+1 {{raw pointer or reference type}}
  int *ptr;
};

// Hidden behind a typedef is still a raw pointer (canonical-type check).
// expected-error@+1 {{raw pointer or reference type}}
typedef int *IntPtr;

static void bad_local() noexcept {
  // expected-error@+1 {{raw pointer or reference type}}
  int *p = nullptr;
  (void)p;
  int x = 0;
  // expected-error@+1 {{raw pointer or reference type}}
  int &r = x;
  (void)r;
  // expected-error@+1 {{raw pointer or reference type}}
  int *arr[2];
  (void)arr;
}

struct S { int m; };
// expected-error@+1 {{raw pointer or reference type}}
static int S::*bad_member_ptr = nullptr;

// [[unsafe]] on a FUNCTION is a call restriction, not an unsafe body or
// signature: the declaration is still safe source.
// expected-error@+1 {{raw pointer or reference type}}
[[unsafe]] static int *unsafe_fn_still_banned() noexcept;

// --- allowed inside [[unsafe]] fences ----------------------------------------

// A function-body fence.
static void body_fence() noexcept {
  [[unsafe]] {
    int *p = nullptr;
    (void)p;
    int x = 0;
    int &r = x;
    (void)r;
  }
}

// A namespace-scope fence is scope-transparent, so these declarations remain
// usable from safe source while their internals are trusted-unsafe.
[[unsafe]] {
  static int *fenced_return() noexcept { return nullptr; }
  static void fenced_params(int *p, int &r) noexcept { (void)p; (void)r; }
  struct FencedStruct { int *ptr; };
  static int *fenced_global;
  typedef int *FencedPtr;
}

static void use_fenced() noexcept {
  (void)fenced_return();
  FencedStruct fs{};
  fenced_params(fs.ptr, *fenced_global);
}

// --- d0708 parameters are unaffected by the ban ------------------------------

struct Big { char buf[64]; };
static void d0708(in int x, out int y, inout int z, move int w) noexcept {
  y = x + z;
  (void)w;
}
static void d0708_big(in Big b) noexcept { (void)b; }

// --- range-for ---------------------------------------------------------------
// Hidden __range/__begin/__end variables are compiler-generated, so they are
// not checked; a by-value loop variable is fine.
[[unsafe]] {
struct V { int a[4]; int *begin() noexcept { return a; } int *end() noexcept { return a + 4; } };
}
static void range_value(inout V v) noexcept {
  for (auto x : v) (void)x;  // OK: copy
}
static void range_ref(inout V v) noexcept {
  // expected-error@+1 {{raw pointer or reference type}}
  for (auto &x : v) (void)x;
}
static void range_in(inout V v) noexcept {
  // expected-error@+1 {{unknown type name 'in'}}
  for (in auto x : v) (void)x;
}
