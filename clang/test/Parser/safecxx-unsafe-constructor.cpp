// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// [[unsafe]] on a constructor is a call restriction. CXXConstructExpr is not a
// CallExpr, so this needs the constructor-specific check in BuildCXXConstructExpr.

[[unsafe]] {
struct S {
  S() noexcept {}
  S(const S &) noexcept {}
  [[unsafe]] S(S &&) noexcept {}
  [[unsafe]] S &operator=(S &&) noexcept { return *this; }
};
}

static void safe_copy(inout S a) noexcept {
  S b = a; // OK: copy constructor is safe
  (void)b;
}

static void unsafe_move(inout S a) noexcept {
  S b = static_cast<S &&>(a); // expected-error {{call to 'S' is unsafe}}
  (void)b;
}

static void unsafe_move_assign(inout S a, inout S b) noexcept {
  a = static_cast<S &&>(b); // expected-error {{call to 'operator=' is unsafe}}
}

static void fenced_move(inout S a) noexcept {
  [[unsafe]] {
    S b = static_cast<S &&>(a); // OK: inside the fence
    (void)b;
  }
}
