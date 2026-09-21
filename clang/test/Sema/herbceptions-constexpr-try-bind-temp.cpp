// RUN: %clang_cc1 -std=c++26 -fherbceptions -fsyntax-only -verify %s
// expected-no-diagnostics

// Regression test: constant-evaluating a `try(expr)` whose operand is a call
// returning a class prvalue. The call is wrapped in a CXXBindTemporaryExpr,
// which IgnoreParenImpCasts does not look through. The evaluator must strip
// implicit nodes (temporary binding, materialization, cleanups) before
// casting to CallExpr; previously the unchecked cast in VisitCXXTryExpr read
// a garbage callee pointer and crashed.

struct str {
  const char *p;
  constexpr ~str() {}
};

constexpr str make_str() throws {
  return str{nullptr};
}

// Inside a throws function a bare call to make_str() is auto-wrapped in
// try(...); the class prvalue result carries a CXXBindTemporaryExpr.
// The constexpr destructor makes Sema constant-evaluate the initializer of
// `s` (FinalizeVarWithDestructor -> VarDecl::evaluateValue), which used to
// crash in VisitCXXTryExpr.
constexpr void use_try() throws {
  auto s = make_str();
  (void)s;
}

// Same through an explicit try(...) with a class prvalue result.
constexpr void use_explicit_try() throws {
  auto s = try(make_str());
  (void)s;
}

// And through a function template instantiation, which is how the crash was
// originally observed.
template <typename T>
constexpr void use_try_templated() throws {
  auto s = make_str();
  (void)s;
}

void instantiate() throws {
  use_try_templated<int>();
}
