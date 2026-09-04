// REQUIRES: native, x86-registered-target
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 \
// RUN:   -fherbceptions -emit-llvm -o %t.ll %s
// RUN: FileCheck %s --input-file=%t.ll
// RUN: opt -passes=verify -disable-output %t.ll
// RUN: %clangxx -std=c++20 -fherbceptions -fno-exceptions -O0 %s -o %t.o0
// RUN: %t.o0
// RUN: %clangxx -std=c++20 -fherbceptions -fno-exceptions -O2 %s -o %t.o2
// RUN: %t.o2

// Reference binding and reference consumption are different operations. A
// shaped reference result is an address; a by-value consumer must load the
// referent after successful dispatch, preserving ordinary volatile, boolean,
// pointer, and complex memory semantics. Each operation below also checks
// that neither the success load nor the failure edge re-evaluates its source.

using complex_double = _Complex double;

int calls;
int scalar_value = 41;
int *pointer_value = &scalar_value;
bool boolean_value = true;
volatile int volatile_value = 59;
complex_double complex_value;

int &int_lref(bool fail) return_failure{int} {
  ++calls;
  if (fail)
    return_failure 23;
  return scalar_value;
}

int &&int_xref(bool fail) return_failure{int} {
  ++calls;
  if (fail)
    return_failure 23;
  return static_cast<int &&>(scalar_value);
}

int *&pointer_lref(bool fail) return_failure{int} {
  ++calls;
  if (fail)
    return_failure 23;
  return pointer_value;
}

int *&&pointer_xref(bool fail) return_failure{int} {
  ++calls;
  if (fail)
    return_failure 23;
  return static_cast<int *&&>(pointer_value);
}

bool &bool_lref(bool fail) return_failure{int} {
  ++calls;
  if (fail)
    return_failure 23;
  return boolean_value;
}

volatile int &&volatile_xref(bool fail) return_failure{int} {
  ++calls;
  if (fail)
    return_failure 23;
  return static_cast<volatile int &&>(volatile_value);
}

complex_double &complex_lref(bool fail) return_failure{int} {
  ++calls;
  if (fail)
    return_failure 23;
  return complex_value;
}

complex_double &&complex_xref(bool fail) return_failure{int} {
  ++calls;
  if (fail)
    return_failure 23;
  return static_cast<complex_double &&>(complex_value);
}

// CHECK-LABEL: define{{.*}} @_Z{{[0-9]+}}copy_int_lrefb(
// CHECK: call {{.*}} @_Z{{[0-9]+}}int_lrefb(
// CHECK: br i1
// CHECK: load i32, ptr
// CHECK: add nsw i32
int copy_int_lref(bool fail) return_failure{int} {
  return try(int_lref(fail)) + 1;
}

// CHECK-LABEL: define{{.*}} @_Z{{[0-9]+}}copy_int_xrefb(
// CHECK: call {{.*}} @_Z{{[0-9]+}}int_xrefb(
// CHECK: br i1
// CHECK: load i32, ptr
// CHECK: add nsw i32
int copy_int_xref(bool fail) return_failure{int} {
  return int_xref(fail) + 2;
}

// A missing pointer-reference load is LLVM-type-correct but returns the
// address of pointer_value instead of its stored pointer. The native checks
// below therefore compare identity with &scalar_value, not merely IR types.
int *copy_pointer_lref(bool fail) return_failure{int} {
  return pointer_lref(fail);
}

int *copy_pointer_xref(bool fail) return_failure{int} {
  return try(pointer_xref(fail));
}

// CHECK-LABEL: define{{.*}} @_Z{{[0-9]+}}copy_bool_lrefb(
// CHECK: call {{.*}} @_Z{{[0-9]+}}bool_lrefb(
// CHECK: br i1
// CHECK: load i8, ptr
bool copy_bool_lref(bool fail) return_failure{int} {
  return try(bool_lref(fail));
}

// CHECK-LABEL: define{{.*}} @_Z{{[0-9]+}}copy_volatile_xrefb(
// CHECK: call {{.*}} @_Z{{[0-9]+}}volatile_xrefb(
// CHECK: br i1
// CHECK: load volatile i32, ptr
int copy_volatile_xref(bool fail) return_failure{int} {
  return volatile_xref(fail);
}

// CHECK-LABEL: define{{.*}} @_Z{{[0-9]+}}copy_complex_lrefb(
// CHECK: call {{.*}} @_Z{{[0-9]+}}complex_lrefb(
// CHECK: br i1
// CHECK: load double, ptr
// CHECK: load double, ptr
complex_double copy_complex_lref(bool fail) return_failure{int} {
  return try(complex_lref(fail));
}

// CHECK-LABEL: define{{.*}} @_Z{{[0-9]+}}copy_complex_xrefb(
// CHECK: call {{.*}} @_Z{{[0-9]+}}complex_xrefb(
// CHECK: br i1
// CHECK: load double, ptr
// CHECK: load double, ptr
complex_double copy_complex_xref(bool fail) return_failure{int} {
  return complex_xref(fail);
}

#define CHECK_VALUE(Function, Expected)                                         \
  do {                                                                         \
    calls = 0;                                                                 \
    auto success = catch return_failure(Function(false));                       \
    if (success.failed || success.value != (Expected) || calls != 1)             \
      return __LINE__;                                                         \
    calls = 0;                                                                 \
    auto failure = catch return_failure(Function(true));                        \
    if (!failure.failed || failure.error != 23 || calls != 1)                    \
      return __LINE__;                                                         \
  } while (false)

int main() {
  __real__ complex_value = 3.0;
  __imag__ complex_value = 4.0;
  CHECK_VALUE(copy_int_lref, 42);
  CHECK_VALUE(copy_int_xref, 43);
  CHECK_VALUE(copy_pointer_lref, &scalar_value);
  CHECK_VALUE(copy_pointer_xref, &scalar_value);
  CHECK_VALUE(copy_bool_lref, true);
  CHECK_VALUE(copy_volatile_xref, 59);
  CHECK_VALUE(copy_complex_lref, complex_value);
  CHECK_VALUE(copy_complex_xref, complex_value);
}

#undef CHECK_VALUE
