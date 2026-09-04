// RUN: %clang_cc1 -std=c++20 -fherbceptions -fsyntax-only -verify %s

// The deterministic discriminator is orthogonal to a successful reference
// result.  Automatic and explicit propagation must preserve both reference
// identity and the lvalue/xvalue category used by decltype(auto) deduction.

struct value {};
value global;

value &source_lvalue() throws { return global; }
value &&source_xvalue() throws { return static_cast<value &&>(global); }

decltype(auto) automatic_lvalue() throws { return source_lvalue(); }
decltype(auto) automatic_xvalue() throws { return source_xvalue(); }
decltype(auto) explicit_lvalue() throws { return try(source_lvalue()); }
decltype(auto) explicit_xvalue() throws { return try(source_xvalue()); }

static_assert(__is_same(decltype(automatic_lvalue()), value &));
static_assert(__is_same(decltype(automatic_xvalue()), value &&));
static_assert(__is_same(decltype(explicit_lvalue()), value &));
static_assert(__is_same(decltype(explicit_xvalue()), value &&));

int select(value &);
long select(value &&);

void classification_contract() throws {
  // These operations go through ExprClassification rather than merely
  // reading Expr::getValueKind(), so they catch a stale hard-coded prvalue
  // classification for CXXTryExpr.
  (try(source_lvalue())) = value{};
  static_assert(__is_same(decltype(select(try(source_lvalue()))), int));
  static_assert(__is_same(decltype(select(try(source_xvalue()))), long));
}

template <class Function>
decltype(auto) dependent_explicit(Function function) throws {
  return try(function());
}

static_assert(
    __is_same(decltype(dependent_explicit(&source_lvalue)), value &));
static_assert(
    __is_same(decltype(dependent_explicit(&source_xvalue)), value &&));

using lvalue_function_pointer = value &() throws;
using xvalue_function_pointer = value &&() throws;

decltype(auto) indirect_lvalue(lvalue_function_pointer *function) throws {
  return try(function());
}
decltype(auto) indirect_xvalue(xvalue_function_pointer *function) throws {
  return try(function());
}
static_assert(__is_same(decltype(indirect_lvalue(nullptr)), value &));
static_assert(__is_same(decltype(indirect_xvalue(nullptr)), value &&));

// expected-no-diagnostics
