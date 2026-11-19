// RUN: %clang_cc1 -fsyntax-only -fcxx-exceptions -fsafecxx -verify %s

// Legacy EH and RTTI are banned in safe source, but compile normally inside
// a [[unsafe]] { } fence. -fsafecxx does NOT imply -fno-exceptions/-fno-rtti.

// Legacy throw.
static void bad_throw() noexcept {
  // expected-error@+1 {{legacy C++ 'throw' is not allowed in safe C++ source}}
  throw 1;
}

static void ok_throw() noexcept {
  [[unsafe]] {
    throw 1;  // OK inside the fence
  }
}

// Legacy try/catch.
static void bad_try() noexcept {
  try {
    // expected-error@+1 {{legacy C++ 'throw' is not allowed in safe C++ source}}
    throw 1;
  } catch (int) { // expected-error {{legacy C++ 'catch' is not allowed in safe C++ source}}
    // The legacy catch is rejected, so the enclosing try is never built.
  }
}

static void ok_try() noexcept {
  [[unsafe]] {
    try {
      throw 1;
    } catch (int) {
    }
  }
}

// RTTI.
struct Base {
  virtual ~Base() noexcept {}
  Base() noexcept = default;
  Base(in Base) = delete;
  Base operator=(in Base) = delete;
};
struct Derived : Base {};

// inout lowers to Base&, so no raw reference is named in safe source.
static void bad_rtti(inout Base p) noexcept {
  // expected-error@+1 {{'dynamic_cast' is not allowed in safe C++ source}}
  (void)dynamic_cast<Derived *>(&p);
  // expected-error@+1 {{'typeid' is not allowed in safe C++ source}}
  (void)typeid(p);
}

static void ok_rtti(inout Base p) noexcept {
  [[unsafe]] {
    (void)dynamic_cast<Derived *>(&p);
    // expected-error@+1 {{you need to include <typeinfo> before using the 'typeid' operator}}
    (void)typeid(p);
  }
}
