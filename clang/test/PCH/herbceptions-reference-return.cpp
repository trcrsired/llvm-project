// RUN: %clang_cc1 -std=c++20 -fherbceptions -emit-pch -o %t %s
// RUN: %clang_cc1 -std=c++20 -fherbceptions -include-pch %t -fsyntax-only -verify %s

#ifndef HERBCEPTIONS_REFERENCE_RETURN_HEADER
#define HERBCEPTIONS_REFERENCE_RETURN_HEADER

struct value {};
extern value global;

value &source_lvalue() throws;
value &&source_xvalue() throws;

inline decltype(auto) serialized_lvalue() throws {
  return try(source_lvalue());
}
inline decltype(auto) serialized_xvalue() throws {
  return try(source_xvalue());
}

#else

static_assert(__is_same(decltype(serialized_lvalue()), value &));
static_assert(__is_same(decltype(serialized_xvalue()), value &&));

inline void serialized_classification() throws {
  // Exercise the value kind restored by ASTReader, not a freshly rebuilt
  // CXXTryExpr from this translation unit.
  (try(source_lvalue())) = value{};
}

// expected-no-diagnostics

#endif
