// RUN: %clang_cc1 -std=c++20 -fherbceptions -fno-cxx-exceptions \
// RUN:   -fsyntax-only -verify %s

int active() throws;
int active_true() throws(true);
int disabled() throws(false);
int ordinary() noexcept;
int may_unwind();

// These pairs have one physical ABI each.
auto active_pair = true ? &active : &active_true;
auto ordinary_pair = true ? &disabled : &ordinary;
static_assert(__is_same(decltype(active_pair), int (*)() throws));
static_assert(__is_same(decltype(ordinary_pair), int (*)() noexcept));

// Unlike noexcept qualification, a live channel cannot be added to the plain
// function pointer: the two callees return different LLVM types.
auto mismatched_pair = true ? &active : &ordinary; // expected-error {{incompatible operand types}}

template <bool Enabled>
using conditional_pointer = int (*)() throws(Enabled);

template <bool Enabled>
auto dependent_mismatch(conditional_pointer<Enabled> lhs, int (*rhs)()) {
  return true ? lhs : rhs; // expected-error {{incompatible operand types}}
}

// Substitution must resolve the conditional channel before computing the
// composite pointer type: false has the ordinary ABI, while true is shaped.
auto dependent_false = dependent_mismatch<false>(&disabled, &may_unwind);
static_assert(__is_same(decltype(dependent_false), int (*)()));
auto dependent_true = dependent_mismatch<true>(&active, &may_unwind); // expected-note {{in instantiation of function template specialization 'dependent_mismatch<true>' requested here}}
