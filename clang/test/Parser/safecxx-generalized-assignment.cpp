// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// d0708 generalized value-setting: a class may spell its assignment operator
// with borrow kinds instead of raw references. `inout T` lowers to T&, and
// `return this;` is the safe spelling of `return *this;`. A plain assignment
// to an out parameter sets its value (it is not a read of it).

struct Str {
  int n = 0;

  inout Str get() noexcept { return this; } // other inout-returning methods too
  inout Str set(in Str other) noexcept;

  inout Str operator=(in Str other) noexcept {
    n = other.n;
    return this; // OK: returns the borrowed object
  }
};

// Out-of-class definitions use the same spelling.
inout Str Str::set(in Str other) noexcept {
  n = other.n;
  return this;
}

static void use(in int) noexcept {}

static void user_defined(out Str s) noexcept {
  s = Str{};    // construction through operator=
  use(s.n);     // OK
}

struct Data {
  int n;
};

static void implicit(out Data s) noexcept {
  s = Data{};   // implicit operator= also sets the value
  use(s.n);
}

// Compound assignment reads the object first, so it cannot construct it.
struct Add {
  int n = 0;
  inout Add operator+=(in Add other) noexcept {
    n += other.n;
    return this;
  }
};

static void compound(out Add a) noexcept {
  a += Add{}; // expected-error {{out parameter 'a' is read before it is constructed}}
  use(a.n);
}

// A write to a member is not whole-object value-setting: the class's
// invariants are the class author's business, not the analyzer's.
struct Pair {
  int a;
  int b;
};
static void member_write(out Pair p) noexcept {
  p.a = 1;    // expected-error {{out parameter 'p' is read before it is constructed}}
  use(p.a);
}

// The raw-reference spelling remains banned in safe source.
struct Raw {
  Raw &operator=(const Raw &) noexcept { // expected-error 2 {{raw pointer or reference type}}
    return *this;                        // expected-error {{dereference of a pointer}}
  }
};
