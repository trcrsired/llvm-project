// RUN: %clang_cc1 -std=c++26 -fherbceptions -fsyntax-only -verify %s

// Deallocation functions cannot be declared with a herbceptions exception
// specification: they run on cleanup/unwinding paths (e.g. when construction
// inside a new-expression fails) where no frame can consume an error result.
// Allocation functions, by contrast, may propagate through the channel.

struct Ok {
  static void *operator new(__SIZE_TYPE__) throws;
  static void operator delete(void *) noexcept;
};

struct DeleteThrows {
  static void *operator new(__SIZE_TYPE__) noexcept;
  static void operator delete(void *) throws;
  // expected-error@-1 {{deallocation function cannot be declared with a herbceptions ('throws'/'return_failure{...}') exception specification; deallocation runs on cleanup/unwinding paths that cannot propagate errors}}
};

struct DeleteArrayThrows {
  static void *operator new[](__SIZE_TYPE__) noexcept;
  static void operator delete[](void *) throws;
  // expected-error@-1 {{deallocation function cannot be declared with a herbceptions ('throws'/'return_failure{...}') exception specification; deallocation runs on cleanup/unwinding paths that cannot propagate errors}}
};

struct DeleteReturnFailure {
  static void *operator new(__SIZE_TYPE__) noexcept;
  static void operator delete(void *) return_failure{int};
  // expected-error@-1 {{deallocation function cannot be declared with a herbceptions ('throws'/'return_failure{...}') exception specification; deallocation runs on cleanup/unwinding paths that cannot propagate errors}}
  // expected-error@-2 {{'return_failure{...}' is a C-style feature and may only be attached to free (non-member) functions; it is not allowed on member functions (including static members), lambdas, or function templates}}
};

// The global replaceable operator delete already has an implicit
// declaration; a 'throws' declaration is a different type, so it does not
// overload the implicit one (and is rejected before the deallocation check
// is even reached).
void *operator delete(void *) throws;
// expected-error@-1 {{functions that differ only in their return type cannot be overloaded}}
// expected-note@-2 {{previous implicit declaration is here}}
