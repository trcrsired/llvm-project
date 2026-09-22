// RUN: %clang_cc1 -fherbceptions -fsyntax-only -verify %s

/* In C, try(expr) is mandatory around a return_failure{E} call — even inside
   another return_failure function. C has no 'throws' functions, so there is
   no implicit propagation like C++ has. */

enum errc : unsigned { errc_ok };

// expected-note@+3 {{function declared with 'return_failure{...}' here}}
// expected-note@+2 {{function declared with 'return_failure{...}' here}}
// expected-note@+1 {{function declared with 'return_failure{...}' here}}
int f(int x) return_failure{enum errc};

int ok_in_return_failure(int x) return_failure{enum errc} {
  return try(f(x));
}

int bad_in_return_failure(int x) return_failure{enum errc} {
  // expected-error@+1 {{calling function with 'return_failure{...}' specifier requires 'try()' or 'catch return_failure()' wrapper}}
  return f(x);
}

int bad_plain(int x) {
  // expected-error@+1 {{calling function with 'return_failure{...}' specifier requires 'try()' or 'catch return_failure()' wrapper}}
  return f(x);
}

/* try{} / catch are C++-only: in C exceptions are disabled, so the statement
   form is rejected entirely. */
int bad_try_block(int x) {
  // expected-error@+2 {{cannot use 'try' with exceptions disabled}}
  // expected-error@+1 {{calling function with 'return_failure{...}' specifier requires 'try()' or 'catch return_failure()' wrapper}}
  try { return f(x); }
  catch (int e) { return e; }
}
