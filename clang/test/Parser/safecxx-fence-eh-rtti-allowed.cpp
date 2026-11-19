// RUN: %clang_cc1 -fsyntax-only -fcxx-exceptions %s
// RUN: %clang_cc1 -fsyntax-only -fcxx-exceptions -fsafecxx-unsafe %s

// Without the -fsafecxx umbrella, legacy EH/RTTI are unaffected -- the ban
// is a source-level rule of safe C++ only, not a change to -fexceptions or
// -fno-rtti. [[unsafe]] alone does not enable any restriction.

void legacy() {
  try {
    throw 1;
  } catch (int) {
  }
}

struct Base { virtual ~Base() {} };
struct Derived : Base {};

void rtti(Base *p) {
  (void)dynamic_cast<Derived *>(p);
}
