// RUN: %clang_cc1 -std=c++20 -fherbceptions -emit-llvm -o - %s | FileCheck %s

// A complex-valued try-expression must use the real and imaginary components
// decoded from its one shaped call.  In particular, its failure edge must
// forward the already returned union payload; re-emitting the operand would
// increment complex_calls twice and could overflow a narrower caller payload.

typedef _Complex double complex_double;

int complex_calls;

complex_double complex_observed(bool fail) return_failure{int} {
  ++complex_calls;
  if (fail)
    return_failure 23;
  complex_double value;
  __real__ value = 3.0;
  __imag__ value = 4.0;
  return value;
}

// The constant false selects the successful dynamic outcome.  The generated
// control flow still contains the failure edge, so CHECK-NOT also proves that
// neither edge contains a second evaluation of the side-effecting operand.
// CHECK-LABEL: define{{.*}} @_Z15complex_successv(
// CHECK: call {{.*}} @_Z16complex_observedb(i1{{.*}}false)
// CHECK-NOT: call {{.*}} @_Z16complex_observedb(
// CHECK: fadd double
// CHECK: ret
double complex_success() return_failure{int} {
  complex_double value = try(complex_observed(false));
  return __real__ value + __imag__ value;
}

// The constant true selects the failure outcome at run time.  The one call is
// nevertheless the sole producer for both the propagated error payload and
// the (unreached at run time) success components.
// CHECK-LABEL: define{{.*}} @_Z15complex_failurev(
// CHECK: call {{.*}} @_Z16complex_observedb(i1{{.*}}true)
// CHECK-NOT: call {{.*}} @_Z16complex_observedb(
// CHECK: fadd double
// CHECK: ret
double complex_failure() return_failure{int} {
  complex_double value = try(complex_observed(true));
  return __real__ value + __imag__ value;
}
