// RUN: %clang_cc1 -std=c++26 -fherbceptions -fsyntax-only -verify %s
// expected-no-diagnostics

// The compiler injects the herbceptions error channel ABI type under
// -fherbceptions: the __cxx_std_error typedef names it, and
// __CXX_STD_ERROR_TYPE__ expands to that name so headers and FFI code can
// refer to the type without declaring anything.

#ifndef __CXX_STD_ERROR_TYPE__
#error "expected __CXX_STD_ERROR_TYPE__ to be defined under -fherbceptions"
#endif

using builtin_error = __CXX_STD_ERROR_TYPE__;
using builtin_error2 = __cxx_std_error;
static_assert(__is_same(builtin_error, builtin_error2));

// The layout is the error channel ABI: {void const *domain, uintptr_t code}.
static_assert(sizeof(__CXX_STD_ERROR_TYPE__) == 2 * sizeof(void *));
static_assert(alignof(__CXX_STD_ERROR_TYPE__) == alignof(void *));

__cxx_std_error make(unsigned long c) {
  return __cxx_std_error{nullptr, c};
}

// The builtin type is marked 'herbceptions_cxx_std_error': a `throw throws`
// operand of this type is carried directly as the error value, without an
// error_domain<T> specialization.
void rethrow(__CXX_STD_ERROR_TYPE__ e) throws {
  throw throws e;
}

extern "C" __CXX_STD_ERROR_TYPE__ ffi_error();
void forward() throws {
  throw throws ffi_error();
}
