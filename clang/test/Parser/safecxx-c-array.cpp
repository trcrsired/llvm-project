// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx -verify %s

// C-style arrays are not available in safe C++ source. Mark the declaration
// [[unsafe]], or place it inside a fence, to use one.

static void local() noexcept {
  int a[4]; // expected-error {{C-style array declarations are not allowed}}
  (void)a;
}

[[unsafe]] int fenced_global[4];

static void fenced_local() noexcept {
  [[unsafe]] {
    int a[4];
    (void)a;
  }
  [[unsafe]] int b[4]; // mark on the declaration
  (void)b;
}

struct S {
  int a[4]; // expected-error {{C-style array declarations are not allowed}}
};

static void use() noexcept {
  S s;
  (void)s;
}

// A pointer is fine: it is an opaque token, not an array object.
static void pointer(int *p) noexcept { (void)p; }
