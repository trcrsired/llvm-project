// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// A deleted function cannot be called, so it has no unsafe operations to hide
// in its signature and needs no error channel or linkage marks. The `= delete`
// suffix is read after the declarator, so the exemption is a lexical lookahead
// from the end of the declarator.

static void free_del(int &r) = delete;
struct C {
  C(const C &) = delete;
  C &operator=(const C &) = delete;
  static void member_del(int &r) = delete;
  void member_del2(int *p) /* comment */ = /* comment */ delete;
  void member_del3(int C::*p) = delete;
};

// A class with a user-declared destructor can now state its copy semantics in
// safe source, which is what the mandatory-marks diagnostic asks for.
struct Cleanup {
  Cleanup() noexcept {}
  Cleanup(const Cleanup &) = delete;
  Cleanup &operator=(const Cleanup &) = delete;
  ~Cleanup() noexcept {
    [[unsafe]] { /* legacy cleanup */ }
  }
};

static void use_cleanup(in Cleanup c) noexcept { (void)c; }

// Controls: a declaration that is not deleted still needs its marks.
static void ok(int *p) noexcept { (void)p; }
static void bad_ref(int &r) noexcept { // expected-error {{raw pointer or reference type}}
  (void)r;
}
void bad_linkage() noexcept {} // expected-error {{must declare its linkage}}
