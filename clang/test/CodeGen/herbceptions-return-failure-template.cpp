// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu \
// RUN:   -fherbceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=ITANIUM
// RUN: %clang_cc1 -std=c++20 -triple x86_64-pc-windows-msvc \
// RUN:   -fherbceptions -fms-compatibility-version=19.14 \
// RUN:   -emit-llvm -o - %s | FileCheck %s --check-prefix=MS

struct Error {
  int value;
};

template <class E>
int templated_failure(E error, bool fail) return_failure{E} {
  if (fail)
    return_failure error;
  return 7;
}

template int templated_failure<Error>(Error, bool);

// The specialization retains the typed return carrier instead of silently
// degrading to an ordinary int-returning function.
// ITANIUM: define{{.*}} { i32, i1 } @_Z{{[0-9]+}}templated_failure
// MS: define{{.*}} { i32, i1 } @"??$templated_failure@

template <class T>
struct tag {};

template <class E>
using dependent_failure_type = int(int) return_failure{E};

using hidden_const_error = const int;
using hidden_volatile_error = volatile int;
static_assert(__is_same(dependent_failure_type<hidden_const_error>,
                        dependent_failure_type<int>));
static_assert(__is_same(dependent_failure_type<hidden_volatile_error>,
                        dependent_failure_type<int>));
static_assert(!__is_same(dependent_failure_type<const int *>,
                         dependent_failure_type<int *>));

template <class E>
void observe_typed_dependent(tag<dependent_failure_type<E>> *) {}

template void
observe_typed_dependent<int>(tag<dependent_failure_type<int>> *);
template void
observe_typed_dependent<long>(tag<dependent_failure_type<long>> *);

// Itanium structurally preserves the dependent payload in the primary
// template's function type; Microsoft resolves the alias per specialization.
// ITANIUM-DAG: @_Z23observe_typed_dependentIiEvP3tagIDXFT_EFiiEE
// ITANIUM-DAG: @_Z23observe_typed_dependentIlEvP3tagIDXFT_EFiiEE
// MS-DAG: @"??$observe_typed_dependent@H@@{{.*}}_HTH
// MS-DAG: @"??$observe_typed_dependent@J@@{{.*}}_HTJ
