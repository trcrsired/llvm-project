// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// Pointers are opaque tokens in safe C++: they can be declared, passed,
// stored and compared. Looking through one is confined to [[unsafe]] blocks.
// References remain banned outright.

[[unsafe]] static void unsafe_fn() noexcept {}

struct S {
  int x;
};

static void pass(S *p) noexcept { (void)p; }

static void opaque_ok(S *p, S *q) noexcept {
  pass(p);
  S *r = p;
  if (p == q)
    return;
  (void)r;
}

static void deref_bad(S *p) noexcept {
  (void)*p;   // expected-error {{dereference of a pointer is not allowed}}
  (void)p->x; // expected-error {{dereference of a pointer is not allowed}}
  (void)p[0]; // expected-error {{dereference of a pointer is not allowed}}
}

static void deref_fenced(S *p) noexcept {
  [[unsafe]] {
    (void)*p;
    (void)p->x;
    (void)p[0];
  }
}

// Calling through a function pointer is a dereference too. This also closes
// the indirect route to an [[unsafe]] function: forming the pointer is fine,
// calling it is not.
static void indirect() noexcept {
  auto fp = unsafe_fn;
  fp();        // expected-error {{dereference of a pointer is not allowed}}
  unsafe_fn(); // expected-error {{call to 'unsafe_fn' is unsafe}}
}

// Allocation and deallocation are unsafe acts.
static void allocation() noexcept {
  auto p = new int(1); // expected-error {{'new' and 'delete' are not allowed}}
  delete p;            // expected-error {{'new' and 'delete' are not allowed}}
}

static void allocation_fenced() noexcept {
  [[unsafe]] {
    auto p = new int(1);
    delete p;
  }
}

// References are still banned, including in pointer-heavy signatures.
static void ref_bad(S &r) noexcept {} // expected-error {{raw pointer or reference type}}
