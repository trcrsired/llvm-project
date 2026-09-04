// RUN: %clang_cc1 -std=c++20 -fherbceptions -fsyntax-only -verify %s

struct Payload {
  unsigned value;
};

struct OtherPayload {
  unsigned value;
};

using PayloadAlias = Payload;

// A declaration and its definition retain the dependent typed specification,
// and substitution validates both E and the body operand.
template <class E>
int redeclared(E) return_failure{E};

template <class E>
int redeclared(E value) return_failure{E} {
  return_failure value;
}

template <class E>
using Identity = E;

template <class E>
int aliased(E value) return_failure{Identity<E>} {
  return_failure value;
}

using RedeclaredFunction = decltype(&redeclared<Payload>);
using RedeclaredResult =
    __invoke_herbceptions_return_failure_result<RedeclaredFunction>;
static_assert(__is_invoke_herbceptions_return_failure(RedeclaredFunction));
static_assert(__is_same(typename RedeclaredResult::value_type, int));
static_assert(__is_same(typename RedeclaredResult::error_type, Payload));

template <class E>
using DependentFailureFunction = int(int) return_failure{E};

template <class E>
inline constexpr bool DependentPayloadIsInt =
    __is_same(DependentFailureFunction<E>, DependentFailureFunction<int>);

template <class E>
inline constexpr bool IsDependentFailureFunction =
    __is_invoke_herbceptions_return_failure(DependentFailureFunction<E>);

static_assert(DependentPayloadIsInt<int>);
static_assert(DependentPayloadIsInt<const int>);
static_assert(!DependentPayloadIsInt<long>);
static_assert(IsDependentFailureFunction<int>);
using DependentResult =
    __invoke_herbceptions_return_failure_result<DependentFailureFunction<long>>;
static_assert(__is_same(typename DependentResult::error_type, long));
using ConstDependentResult = __invoke_herbceptions_return_failure_result<
    DependentFailureFunction<const int>>;
static_assert(__is_same(typename ConstDependentResult::error_type, int));

void instantiateMatchingTemplates() {
  auto First = catch return_failure(redeclared(Payload{}));
  auto Second = catch return_failure(aliased(PayloadAlias{}));
  (void)First;
  (void)Second;
}

// E is dependent while the operand is not. The matching specialization is
// valid; the distinct same-size specialization must rebuild the original
// CXXThrowExpr and diagnose against its substituted E.
template <class E>
int fixedOperand() return_failure{E} {
  return_failure Payload{}; // expected-error {{types must match exactly}}
}

void instantiateFixedOperand() {
  auto Good = catch return_failure(fixedOperand<Payload>());
  // expected-note@+1 {{in instantiation of function template specialization}}
  auto Bad = catch return_failure(fixedOperand<OtherPayload>());
  (void)Good;
  (void)Bad;
}

// The complementary case has a concrete E and a dependent operand. It must be
// checked after T has been substituted even if transformation reuses nodes.
template <class T>
int dependentOperand(T Value) return_failure{Payload} {
  return_failure Value; // expected-error {{types must match exactly}}
}

void instantiateDependentOperand() {
  auto Good = catch return_failure(dependentOperand(Payload{}));
  // expected-note@+1 {{in instantiation of function template specialization}}
  auto Bad = catch return_failure(dependentOperand(OtherPayload{}));
  (void)Good;
  (void)Bad;
}

// expected-note@+1 {{previous declaration is here}}
template <class E> int mismatchedRedeclaration(E) return_failure{E};

// expected-error@+1 {{exception specification in declaration does not match}}
template <class E> int mismatchedRedeclaration(E) return_failure{Payload};

struct Incomplete; // expected-note {{forward declaration}}

template <class E> int incompleteErrorType() return_failure{E} { // expected-error {{incomplete type 'Incomplete' is not allowed}} expected-note {{candidate template ignored: substitution failure}}
  return 0;
}

auto *ForceIncomplete = &incompleteErrorType<Incomplete>; // expected-note {{in instantiation of}} expected-error {{address of overloaded function 'incompleteErrorType' does not match required type}}

struct NonTrivial {
  unsigned value;
  NonTrivial(const NonTrivial &) {}
};

template <class E> int nontrivialErrorType() return_failure{E} { // expected-error {{error type 'NonTrivial' must be trivially copyable}} expected-note {{candidate template ignored: substitution failure}}
  return 0;
}

auto *ForceNonTrivial = &nontrivialErrorType<NonTrivial>; // expected-note {{in instantiation of}} expected-error {{address of overloaded function 'nontrivialErrorType' does not match required type}}

template <class E> int referenceErrorType() return_failure{E}; // expected-error {{error type 'int &' must be an object type}} expected-note {{candidate template ignored: substitution failure}}

auto *ForceReference = &referenceErrorType<int &>; // expected-note {{in instantiation of}} expected-error {{address of overloaded function 'referenceErrorType' does not match required type}}
