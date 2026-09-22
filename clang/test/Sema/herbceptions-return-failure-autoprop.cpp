// RUN: %clang_cc1 -std=c++26 -fherbceptions -fsyntax-only -verify %s
// expected-no-diagnostics

/* In C++, a return_failure{E} call propagates automatically inside a 'throws'
   or 'return_failure' function — try() is optional (unlike C, where it is
   mandatory). try() still works for callers that want the explicit fails
   channel. */

enum class ec : unsigned {};

int f() return_failure{ec};

int auto_in_throws() throws { return f(); }
int auto_in_return_failure() return_failure{ec} { return f(); }
int explicit_try_in_throws() throws { return try(f()); }
int explicit_try_in_return_failure() return_failure{ec} { return try(f()); }
