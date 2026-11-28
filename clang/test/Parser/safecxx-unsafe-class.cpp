// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx -verify %s

// [[unsafe]] on a class is a whole-type call restriction: every member,
// including constructors and implicit special members, may only be called
// from inside an [[unsafe]] fence. This is generic; no library is special.

struct [[unsafe]] legacy {
  legacy() noexcept {}
  void mutate() noexcept {}
  int read() const noexcept { return 0; }
};

static void safe_source() noexcept {
  legacy l; // expected-error {{call to 'legacy' is unsafe}}
  l.mutate(); // expected-error {{call to 'mutate' is unsafe}}
  (void)l.read(); // expected-error {{call to 'read' is unsafe}}
}

[[unsafe]] {
static void fenced() noexcept {
  legacy l;
  l.mutate();
  (void)l.read();
}
}
