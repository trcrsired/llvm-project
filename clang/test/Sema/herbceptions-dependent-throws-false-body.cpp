// RUN: %clang_cc1 -std=c++20 -fherbceptions -fno-cxx-exceptions \
// RUN:   -fsyntax-only -verify %s

int always_fallible(int) throws;

// The call is non-dependent, so its implicit CXXTryExpr is created while the
// primary template has a potential channel. Instantiating the false state must
// revalidate that wrapper and reject propagation into an ordinary ABI.
template <bool Enabled>
int bad_forwarder() throws(Enabled) {
  return always_fallible(0); // expected-error {{'try' is only allowed inside a function declared 'throws' or 'return_failure{...}'}}
}

template int bad_forwarder<false>(); // expected-note {{in instantiation of function template specialization 'bad_forwarder<false>' requested here}}
