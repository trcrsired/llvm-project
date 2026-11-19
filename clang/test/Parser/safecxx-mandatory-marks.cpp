// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// Safe C++ requires every function to declare its error channel and, at
// namespace scope, its linkage explicitly.
void no_marks() {} // expected-error {{must declare its error channel}} expected-error {{must declare its linkage}}
static void ok_noexcept() noexcept {}
static void ok_throws() throws {}
inline void ok_inline() noexcept {}
extern void ok_extern() noexcept {}
namespace { void ok_anon() noexcept {} }

// Every virtual function (and every override) must be explicitly marked.
// Copy operations use the d0708 spelling so no raw reference is named in
// safe source; they still lower to the usual const& signatures.
struct Base {
  virtual void f() noexcept {}
  virtual ~Base() noexcept = default;
  Base() noexcept = default;
  Base(in Base) = delete;
  Base operator=(in Base) = delete;
};

struct Derived : Base {
  void f() noexcept override {}
};

struct Unmarked : Base {
  void f() noexcept {} // expected-error {{must be marked 'virtual', 'override', or 'final'}}
  Unmarked() noexcept = default;
  Unmarked(in Unmarked) = delete;
  Unmarked operator=(in Unmarked) = delete;
};

// Rule of Five: a destructor requires explicit copy operations.
struct BadRule { // expected-error {{not a copy constructor}} expected-error {{not a copy assignment operator}}
  ~BadRule() noexcept {}
};

struct GoodRule {
  ~GoodRule() noexcept {}
  GoodRule(in GoodRule) = delete;
  GoodRule operator=(in GoodRule) = delete;
};

// Inside a fence, none of these restrictions apply to declarations.
static void fence() noexcept {
  [[unsafe]] {
    struct Legacy {
      virtual ~Legacy() {}
      void unmarked() {}
    };
  }
}
