// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// A destructor runs implicitly at every end of lifetime, so it cannot have a
// call-site fence. Unsafe work goes in an [[unsafe]] block inside its body, or
// the whole legacy class goes in a fence. [[unsafe]] on the destructor itself
// is never allowed, not even on a declaration from a fenced header.

[[unsafe]] {
struct Fenced {
  [[unsafe]] ~Fenced() noexcept; // expected-error {{[[unsafe]] is not allowed on a destructor}}
};

struct FencedDefined {
  [[unsafe]] ~FencedDefined() noexcept {} // expected-error {{[[unsafe]] is not allowed on a destructor}}
};
}

// A normal destructor may contain fenced implementation statements. The class
// is fenced only because a user-declared destructor forces explicit copy/move
// declarations, whose raw-reference spelling is not valid in safe source.
[[unsafe]] {
struct Good {
  Good() noexcept {}
  Good(const Good &) = delete;
  Good &operator=(const Good &) = delete;
  ~Good() noexcept {
    [[unsafe]] { /* legacy cleanup here */ }
  }
};
}

static void use() noexcept {
  Good g;      // OK: implicit destruction needs no fence
  g.~Good();   // OK: destructor is not [[unsafe]]
}

// The attribute on other functions is unchanged.
struct Ctor {
  [[unsafe]] explicit Ctor(int) noexcept {}
};
static void ctor() noexcept {
  [[unsafe]] { Ctor c(1); (void)c; }
}
