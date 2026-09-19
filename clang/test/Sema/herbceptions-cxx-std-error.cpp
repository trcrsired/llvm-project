// RUN: %clang_cc1 -std=c++26 -fherbceptions -fsyntax-only -verify %s

// The attribute is visible to all the feature-test spellings:
// __has_cpp_attribute (CXX11 syntax), __has_attribute (GNU syntax), and the
// __-prefixed/__-suffixed forms normalize to the same attribute.
#if !__has_cpp_attribute(__gnu__::__herbceptions_cxx_std_error__)
#error "expected __has_cpp_attribute(__gnu__::__herbceptions_cxx_std_error__)"
#endif
#if !__has_cpp_attribute(gnu::herbceptions_cxx_std_error)
#error "expected __has_cpp_attribute(gnu::herbceptions_cxx_std_error)"
#endif
#if !__has_attribute(__herbceptions_cxx_std_error__)
#error "expected __has_attribute(__herbceptions_cxx_std_error__)"
#endif

// All spellings mark the struct: scoped C++11, double-underscore forms, and
// the GNU __attribute__ spelling.
struct [[gnu::herbceptions_cxx_std_error]] A { void *domain; __SIZE_TYPE__ code; };
struct [[__gnu__::__herbceptions_cxx_std_error__]] B { const void *domain; __SIZE_TYPE__ code; };
struct __attribute__((herbceptions_cxx_std_error)) C { void *domain; __SIZE_TYPE__ code; };

extern "C" struct A get_a();
extern "C" struct B get_b();
extern "C" struct C get_c();

void ffi() throws {
  throw throws get_a();
  throw throws get_b();
  throw throws get_c();
}

// The attribute only applies to structs.
union [[gnu::herbceptions_cxx_std_error]] BadUnion { void *d; __SIZE_TYPE__ c; };
// expected-error@-1 {{'gnu::herbceptions_cxx_std_error' attribute only applies to structs}}

[[gnu::herbceptions_cxx_std_error]] int bad_var;
// expected-error@-1 {{'gnu::herbceptions_cxx_std_error' attribute only applies to structs}}

// Wrong layouts are diagnosed at the definition.
struct [[gnu::herbceptions_cxx_std_error]] TooMany { void *d; __SIZE_TYPE__ c; int extra; };
// expected-error@-1 {{marked 'herbceptions_cxx_std_error' must have the std::error ABI layout}}

struct [[gnu::herbceptions_cxx_std_error]] TooFew { void *d; };
// expected-error@-1 {{marked 'herbceptions_cxx_std_error' must have the std::error ABI layout}}

struct [[gnu::herbceptions_cxx_std_error]] WrongOrder { __SIZE_TYPE__ c; void *d; };
// expected-error@-1 {{marked 'herbceptions_cxx_std_error' must have the std::error ABI layout}}

struct [[gnu::herbceptions_cxx_std_error]] NarrowCode { void *d; int c; };
// expected-error@-1 {{marked 'herbceptions_cxx_std_error' must have the std::error ABI layout}}

struct [[gnu::herbceptions_cxx_std_error]] NotPointer { __SIZE_TYPE__ d; __SIZE_TYPE__ c; };
// expected-error@-1 {{marked 'herbceptions_cxx_std_error' must have the std::error ABI layout}}

// The attribute attached by a redeclaration still applies to the definition.
struct D;
struct [[gnu::herbceptions_cxx_std_error]] D;
struct D { void *domain; __SIZE_TYPE__ code; };
extern "C" struct D get_d();
void ffi2() throws { throw throws get_d(); }

// An unmarked struct with the right layout still needs error_domain.
struct PlainError { void *domain; __SIZE_TYPE__ code; };
void no_attr() throws {
  throw throws PlainError{}; // expected-error {{has no std::error_domain specialization}}
}
