// RUN: %clang_cc1 -std=c++20 -fherbceptions -fcxx-exceptions \
// RUN:   -fsyntax-only -verify %s

// A missing dependent specification cannot copy the old expression: that AST
// refers to the old template parameter. Diagnose and invalidate the new
// declaration without constructing a condition-less dependent function type.
template <bool Enabled>
void missing_dependent() throws(Enabled); // expected-note {{previous declaration is here}}
template <bool Enabled>
void missing_dependent(); // expected-error {{missing exception specification 'throws(Enabled)'}}

template <bool Enabled>
void mismatched_dependent() throws(Enabled); // expected-note {{previous declaration is here}}
template <bool Enabled>
void mismatched_dependent() throws(!Enabled); // expected-error {{exception specification in declaration does not match previous declaration}}
