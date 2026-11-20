// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// A function declared in safe source whose signature names a raw pointer or
// reference is implicitly [[unsafe]] -- a call restriction only. Its body is
// still checked as safe source. Fenced legacy declarations are exempt.

static void raw_param(void *p) noexcept { (void)p; }
static int *raw_return() noexcept;

static void caller() noexcept {
  int x = 0;
  raw_param(&x);       // expected-error {{call to 'raw_param' is unsafe}}
  (void)raw_return();  // expected-error {{call to 'raw_return' is unsafe}}
  [[unsafe]] {
    raw_param(&x);
    (void)raw_return();
  }
}

// d0708 parameters are not raw references in the source spelling.
static void in_param(in int x) noexcept { (void)x; }
static void call_in() noexcept { in_param(1); }

// Declarations in a fenced legacy header stay callable from safe source.
[[unsafe]] {
static void fenced_raw(void *p) noexcept { (void)p; }
static int *fenced_return() noexcept;
}
static void call_fenced() noexcept {
  fenced_raw(nullptr);
  (void)fenced_return();
}
