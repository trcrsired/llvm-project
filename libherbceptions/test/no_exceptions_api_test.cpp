//===--- no_exceptions_api_test.cpp - -fno-exceptions API surface ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The exception_ptr domain ABI entry points and the header-only cast/unbox
// helpers are implemented inside the runtime (always built with EH), so
// consumers compiling with -fno-exceptions must still see their
// declarations. Only APIs that literally throw in the consumer TU
// (throw_legacy_exception*) degrade under -fno-exceptions.
//
// RUN: %cxx -fherbceptions -fno-exceptions -std=c++2c -fsyntax-only \
// RUN:   -I%herbceptions_include %s
// RUN: %cxx -fherbceptions -fno-exceptions -fno-rtti -std=c++2c \
// RUN:   -fsyntax-only -I%herbceptions_include %s
//
//===----------------------------------------------------------------------===//

#include <herbceptions/error>
#include <exception>
#include <typeinfo>

struct api_base {
  virtual ~api_base() = default;
};

int probe(::std::error const &e) {
#if defined(_LIBHERBCEPTIONS_CPP_RTTI)
#ifdef _MSC_VER
  void *p = ::std::error_domains::__cxa_error_code_msvc_exception_ptr_cast(
      e.domain(), e.code(), __builtin_addressof(typeid(api_base)));
#else
  void *p = ::std::error_domains::__cxa_error_code_itanium_exception_ptr_cast(
      e.domain(), e.code(), __builtin_addressof(typeid(api_base)));
#endif
  (void)p;
#endif
  // Available in every configuration; yields nullptr under -fno-rtti.
  api_base *b = ::std::exception_cast_std_error<api_base *>(e);
  (void)b;
  ::std::exception_ptr ep = ::std::make_exception_ptr_from_std_error(e);
  (void)ep;
  return 0;
}
