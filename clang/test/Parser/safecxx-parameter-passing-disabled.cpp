// RUN: %clang_cc1 -fsyntax-only -fno-parameter-passing -verify %s

// Without -fparameter-passing the d0708 words are ordinary identifiers. They
// are not recognized in parameter position, so using one as a type is just an
// unknown type name -- the same as in any ordinary compiler.

// expected-error@+1 {{unknown type name 'in'}}
void f(in int x) noexcept {}

// But they are perfectly fine as variable names.
int in = 0;
int out = 1;
int move = 2;

void use() noexcept {
  int in = 3;
  (void)in;
}
