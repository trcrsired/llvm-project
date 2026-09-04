// RUN: %clang_cc1 -std=c++14 -fherbceptions -fno-cxx-exceptions \
// RUN:   -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++20 -fherbceptions -fno-cxx-exceptions \
// RUN:   -fsyntax-only -verify %s

void active_redeclaration() throws; // expected-note {{previous declaration is here}}
void active_redeclaration(); // expected-error {{missing exception specification 'throws'}}

// Explicit-instantiation matching goes through the FunctionProtoType overload
// and must also retain ABI checks when stack unwinding is disabled.
template <class T>
void explicit_target(T) throws {} // expected-note {{explicit instantiation refers here}}

template void
explicit_target(int) noexcept; // expected-error {{exception specification in explicit instantiation does not match instantiated one}}
