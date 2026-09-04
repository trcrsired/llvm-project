// RUN: %clang_cc1 -std=c++20 -fherbceptions -fcxx-exceptions \
// RUN:   -fsyntax-only -verify %s
// expected-no-diagnostics

template <bool Enabled>
using conditional_type = int(int) throws(Enabled);

using disabled_type = conditional_type<false>;
using enabled_type = conditional_type<true>;
using noexcept_type = int(int) noexcept;
using throws_type = int(int) throws;

// Substitution resolves the condition before canonical type comparison. The
// false specialization has an ordinary noexcept ABI while true has the same
// active effect type as a bare throws specification.
static_assert(__is_same(disabled_type, noexcept_type));
static_assert(__is_same(enabled_type, throws_type));
static_assert(!__is_same(disabled_type, enabled_type));

template <bool Enabled>
int conditional(int value) throws(Enabled) {
  return value + 1;
}

using disabled_pointer = decltype(&conditional<false>);
using enabled_pointer = decltype(&conditional<true>);
static_assert(__is_same(disabled_pointer, int (*)(int) noexcept));
static_assert(__is_same(enabled_pointer, int (*)(int) throws));
static_assert(noexcept(conditional<false>(0)));
static_assert(!noexcept(conditional<true>(0)));
static_assert(!throws(conditional<false>(0)));
static_assert(throws(conditional<true>(0)));
static_assert(!__is_herbceptions_throws_invocable(disabled_pointer, int));
static_assert(__is_herbceptions_throws_invocable(enabled_pointer, int));

template <bool Enabled>
int &conditional_reference() throws(Enabled);

using disabled_reference_pointer = decltype(&conditional_reference<false>);
using enabled_reference_pointer = decltype(&conditional_reference<true>);
static_assert(
    __is_same(disabled_reference_pointer, int &(*)() noexcept));
static_assert(__is_same(enabled_reference_pointer, int &(*)() throws));
static_assert(
    !__is_herbceptions_throws_invocable(disabled_reference_pointer));
static_assert(__is_herbceptions_throws_invocable(enabled_reference_pointer));

// Distinct dependent conditions with the same function signature must retain
// their own expression rather than sharing the first FunctionProtoType sugar.
template <bool Enabled>
int conditional_negated(int) throws(!Enabled);
static_assert(__is_same(decltype(&conditional_negated<false>),
                        int (*)(int) throws));
static_assert(__is_same(decltype(&conditional_negated<true>),
                        int (*)(int) noexcept));

template <bool Enabled> struct condition_token {};

template <bool Enabled>
condition_token<Enabled>
deduce_condition(conditional_type<Enabled> *);

int deduction_active(int) throws;
int deduction_false(int) throws(false);
int deduction_noexcept(int) noexcept;

// Function-type deduction observes the two resolved conditional states. Both
// spellings of the ordinary state deduce false; a live basic channel deduces
// true rather than being treated as a generic nounwind function.
static_assert(__is_same(decltype(deduce_condition(&deduction_active)),
                        condition_token<true>));
static_assert(__is_same(decltype(deduce_condition(&deduction_false)),
                        condition_token<false>));
static_assert(__is_same(decltype(deduce_condition(&deduction_noexcept)),
                        condition_token<false>));

// Equivalent dependent spellings must remain redeclarable before the template
// argument is known; expression profiling supplies that equivalence relation.
template <bool Enabled>
int redeclared(int) throws(Enabled);
template <bool Enabled>
int redeclared(int) throws(Enabled);
