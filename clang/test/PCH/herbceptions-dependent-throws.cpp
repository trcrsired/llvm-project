// RUN: %clang_cc1 -std=c++20 -fherbceptions -emit-pch -o %t %s
// RUN: %clang_cc1 -std=c++20 -fherbceptions -include-pch %t \
// RUN:   -fsyntax-only -verify %s

#ifndef HERBCEPTIONS_DEPENDENT_THROWS_HEADER
#define HERBCEPTIONS_DEPENDENT_THROWS_HEADER

template <bool Enabled>
int serialized_conditional(int) throws(Enabled);

template <bool State>
int serialized_peer(int) throws(State);

template <bool Enabled>
int serialized_negated(int) throws(!Enabled);

template <bool Enabled>
int &serialized_reference() throws(Enabled);

#else

using disabled_pointer = decltype(&serialized_conditional<false>);
using enabled_pointer = decltype(&serialized_conditional<true>);
static_assert(__is_same(disabled_pointer, int (*)(int) noexcept));
static_assert(__is_same(enabled_pointer, int (*)(int) throws));

// The peer has an equivalent profile but a distinct template-parameter AST,
// while the negated declaration has a genuinely different dependent condition.
// Both expressions must survive serialization and substitute independently.
static_assert(__is_same(decltype(&serialized_peer<false>),
                        int (*)(int) noexcept));
static_assert(__is_same(decltype(&serialized_peer<true>), int (*)(int) throws));
static_assert(__is_same(decltype(&serialized_negated<false>),
                        int (*)(int) throws));
static_assert(__is_same(decltype(&serialized_negated<true>),
                        int (*)(int) noexcept));

using disabled_reference_pointer = decltype(&serialized_reference<false>);
using enabled_reference_pointer = decltype(&serialized_reference<true>);
static_assert(
    __is_same(disabled_reference_pointer, int &(*)() noexcept));
static_assert(__is_same(enabled_reference_pointer, int &(*)() throws));

// expected-no-diagnostics

#endif
