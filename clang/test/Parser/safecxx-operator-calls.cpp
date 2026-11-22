// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// [[unsafe]] on a function is a call restriction. A call is still a call when
// it is spelled with brackets, as a conversion, or as an operator, so each
// spelling needs a fence at the call site.

struct View {
  [[unsafe]] int operator[](int i) const noexcept { return i; }
  int operator()(int i) const noexcept { return i; }
};

struct Conv {
  [[unsafe]] operator int() const noexcept { return 0; }
};

static void use(in int) noexcept {}

// operator[]: bracket syntax is a call to an [[unsafe]] function.
static void bad_subscript(View v) noexcept {
  use(v[0]); // expected-error {{call to 'operator[]' is unsafe}}
}
static void good_subscript(View v) noexcept {
  [[unsafe]] { use(v[0]); }
}
static void explicit_subscript(View v) noexcept {
  use(v.operator[](0)); // expected-error {{call to 'operator[]' is unsafe}}
}
struct Plain {
  int operator[](int i) const noexcept { return i; }
};
static void plain_subscript(Plain p) noexcept {
  use(p[0]); // OK: not declared [[unsafe]]
}

// Conversion functions: every context that invokes one is a call site.
static void bad_init(Conv c) noexcept {
  int x = c; // expected-error {{call to 'operator int' is unsafe}}
  use(x);
}
static void bad_arg(Conv c) noexcept {
  use(c); // expected-error {{call to 'operator int' is unsafe}}
}
static void bad_cast(Conv c) noexcept {
  int x = static_cast<int>(c); // expected-error {{call to 'operator int' is unsafe}}
  use(x);
}
static void bad_condition(Conv c) noexcept {
  bool x = c; // expected-error {{call to 'operator int' is unsafe}}
  use(x);
}

// A statement fence covers the call.
static void fenced(Conv c, View v) noexcept {
  [[unsafe]] {
    int x = c;
    use(x);
    use(v[0]);
  }
}

// A [[unsafe]] declaration mark covers only the initializer expression.
static void marked_initializer(Conv c) noexcept {
  [[unsafe]] int x = c;
  use(x); // OK: x is an ordinary safe int afterward
}
