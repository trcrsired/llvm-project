// RUN: %clang_cc1 -std=c++26 -fherbceptions -fsyntax-only -verify %s

// Class-specific operator new may declare 'throws': allocation failure
// propagates through the herbceptions channel to the caller of the
// new-expression, like any other throws call. Deallocation functions may
// not (see herbceptions-deallocation-spec.cpp).

namespace std { struct error { void *domain; __SIZE_TYPE__ code; ~error(); }; }

struct Alloc {
  static void *operator new(__SIZE_TYPE__) throws;
  static void *operator new[](__SIZE_TYPE__) throws;
  static void operator delete(void *) noexcept;
  static void operator delete[](void *) noexcept;
  int v;
};

// A throws function may use the type directly: the new-expression
// auto-propagates the allocation failure through f's own channel.
Alloc *make() throws {
  return new Alloc;
}
Alloc *make_arr() throws {
  return new Alloc[4];
}

// ... or the allocation error can be caught in a try block.
int make_or_negative() {
  try {
    (void)new Alloc;
    return 0;
  } catch throws(std::error e) {
    return -1;
  }
}

// try(expr) does not look through a new-expression to the allocation
// call — use a try block instead.
Alloc *make_try() throws {
  return try(new Alloc); // expected-error {{'try' expression operand must be a call to a function declared 'throws' or 'return_failure{...}'}}
}

// A 'throws' operator new cannot also be 'return_failure{...}' — the two
// specifiers do not coexist, and return_failure is free-functions-only.
struct BadSpec {
  static void *operator new(__SIZE_TYPE__) return_failure{int};
  // expected-error@-1 {{'return_failure{...}' is a C-style feature and may only be attached to free (non-member) functions; it is not allowed on member functions (including static members), lambdas, or function templates}}
};

// constexpr allocation is unaffected: constant expressions only support
// the replaceable *global* ::operator new, never class-specific ones, so
// a 'throws' operator new is unreachable there by construction.

// FIXME: a 'throws' allocation in a non-'throws' function should be
// diagnosed like any other throws call, but CXXNewExpr is not yet checked
// against the enclosing function's spec, so this compiles silently today.
void leak() {
  (void)new Alloc;
}
