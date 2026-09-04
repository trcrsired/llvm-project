// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu \
// RUN:   -fherbceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=ITANIUM
// RUN: %clang_cc1 -std=c++20 -triple x86_64-pc-windows-msvc \
// RUN:   -fherbceptions -fms-compatibility-version=19.14 \
// RUN:   -emit-llvm -o - %s | FileCheck %s --check-prefix=MS

template <class T>
struct tag {};

using plain_type = int(int);
using noexcept_type = int(int) noexcept;
using active_type = int(int) throws;
using active_true_type = int(int) throws(true);
using typed_int_type = int(int) return_failure{int};
using typed_long_type = int(int) return_failure{long};
using hidden_const_error = const int;
using typed_hidden_const_type = int(int) return_failure{hidden_const_error};
using typed_const_pointee_type = int(int) return_failure{const int *};
using typed_pointer_type = int(int) return_failure{int *};

// The explicit true spelling canonicalizes to the same active effect type as
// bare `throws`; the other effect and payload types remain distinct.
static_assert(__is_same(active_type, active_true_type));
static_assert(!__is_same(plain_type, noexcept_type));
static_assert(!__is_same(plain_type, active_type));
static_assert(!__is_same(active_type, typed_int_type));
static_assert(!__is_same(typed_int_type, typed_long_type));
static_assert(__is_same(typed_int_type, typed_hidden_const_type));
static_assert(!__is_same(typed_pointer_type, typed_const_pointee_type));

template <class T>
void instantiate(T *) {}

template void instantiate<plain_type>(plain_type *);
template void instantiate<noexcept_type>(noexcept_type *);
template void instantiate<active_type>(active_type *);
template void instantiate<typed_int_type>(typed_int_type *);
template void instantiate<typed_long_type>(typed_long_type *);

// Before the Herbceptions exception-spec productions were added, all active
// and typed instantiations above selected the plain symbol and were diagnosed
// as multiple definitions with the same mangled name.
// ITANIUM-DAG: @_Z11instantiateIFiiEEvPT_
// ITANIUM-DAG: @_Z11instantiateIDoFiiEEvPT_
// ITANIUM-DAG: @_Z11instantiateIDXHFiiEEvPT_
// ITANIUM-DAG: @_Z11instantiateIDXFiEFiiEEvPT_
// ITANIUM-DAG: @_Z11instantiateIDXFlEFiiEEvPT_
// MS-DAG: @"??$instantiate@$$A6AHH@Z@@YAXP6AHH@Z@Z"
// MS-DAG: @"??$instantiate@$$A6AHH@_E@@YAXP6AHH@_E@Z"
// MS-DAG: @"??$instantiate@$$A6AHH@_HB@@YAXP6AHH@_HB@Z"
// MS-DAG: @"??$instantiate@$$A6AHH@_HTH@@YAXP6AHH@_HTH@Z"
// MS-DAG: @"??$instantiate@$$A6AHH@_HTJ@@YAXP6AHH@_HTJ@Z"

void observe_typed_hidden_const(tag<typed_hidden_const_type> *) {}

// An alias that hides top-level const uses the same canonical payload
// production as `int` on both ABIs.
// ITANIUM-DAG: @_Z26observe_typed_hidden_constP3tagIDXFiEFiiEE
// MS-DAG: @"?observe_typed_hidden_const@@YAXPEAU?$tag@$$A6AHH@_HTH@@@Z"

void observe_active_true(tag<active_true_type> *) {}

// Both active spellings use the canonical DXH / _HB production.
// ITANIUM-DAG: @_Z19observe_active_trueP3tagIDXHFiiEE
// MS-DAG: @"?observe_active_true@@YAXPEAU?$tag@$$A6AHH@_HB@@@Z"

template <bool Enabled>
using conditional_type = int(int) throws(Enabled);

template <bool Enabled>
void observe_dependent(tag<conditional_type<Enabled>> *) {}

template void observe_dependent<false>(tag<conditional_type<false>> *);
template void observe_dependent<true>(tag<conditional_type<true>> *);

// Itanium preserves and structurally mangles the dependent condition. The
// Microsoft ABI resolves this alias at each specialization, so false and true
// select its ordinary noexcept and active encodings respectively.
// ITANIUM-DAG: @_Z17observe_dependentILb0EEvP3tagIDXT_EFiiEE
// ITANIUM-DAG: @_Z17observe_dependentILb1EEvP3tagIDXT_EFiiEE
// MS-DAG: @"??$observe_dependent@$0A@@@YAXPEAU?$tag@$$A6AHH@_E@@@Z"
// MS-DAG: @"??$observe_dependent@$00@@YAXPEAU?$tag@$$A6AHH@_HB@@@Z"
