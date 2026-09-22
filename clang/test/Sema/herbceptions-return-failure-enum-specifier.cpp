// RUN: %clang_cc1 -std=c++26 -fherbceptions -fsyntax-only -verify %s

// 'enum' elaborated specifier inside return_failure{...}: the type parser may
// diagnose the name and hand Sema a null QualType; checkExceptionSpecification
// must not dereference it (formerly an ICE in CheckSpecifiedExceptionType).

enum class ec : unsigned {};
// expected-note@+1 {{declared here}}
typedef ec errc_typedef;
// expected-note@+1 {{declared here}}
using errc_alias = ec;

int a() return_failure{enum ec}; // ok: direct enum tag

// expected-error@+1 {{typedef 'errc_typedef' cannot be referenced with the 'enum' specifier}}
int b() return_failure{enum errc_typedef};

// expected-error@+1 {{type alias 'errc_alias' cannot be referenced with the 'enum' specifier}}
int c() return_failure{enum errc_alias};

// expected-error@+5 {{ISO C++ forbids forward references to 'enum' types}}
// expected-error@+4 {{incomplete type 'enum missing' is not allowed in exception specification}}
// expected-error@+3 {{incomplete type 'enum missing' where a complete type is required}}
// expected-note@+2 {{forward declaration of 'missing'}}
// expected-note@+1 {{forward declaration of 'missing'}}
int d() return_failure{enum missing};
