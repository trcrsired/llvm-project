//===--- itanium_exception_ptr.cpp - itanium exception_ptr domain --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the itanium exception_ptr error_domain_singleton vtable and the
// weak __cxa_error_domain_itanium_exception_ptr ABI entry point. Available on
// all platforms. The code is the thrown-object pointer (the __cxa catch
// value); the vtable releases it on cleanup and rethrows it via the ABI.
//
// The __cxa_* ABI (allocate/free/refcount/rethrow) exists only on Itanium-ABI
// targets (Linux, macOS, Cygwin, MinGW). The MSVC ABI (indicated by _MSC_VER)
// uses __CxxFrameHandler3/_CxxThrowException instead, so the vtable degrades
// gracefully there (no refcount/rethrow support).
//
//===----------------------------------------------------------------------===//

#if !defined(_MSC_VER) || !defined(__cpp_exceptions)
#include "domain_helpers.h"
#include "exception_ptr_itanium.h"
#include <cstddef>
#include <exception>
#include <new>
#include <stdexcept>
#include <system_error>
#include <typeinfo>

namespace std::error_domains {
namespace {
using namespace __herbceptions_detail;

constinit ::std::error_domain_singleton itanium_exception_ptr_domain{
    // The code is the thrown-object pointer (the __cxa catch value). When the
    // error value dies, release the reference; this destroys the exception
    // object exactly when the last reference goes away. On the MSVC ABI there
    // is no __cxa refcount, so cleanup is a no-op.
    .do_cleanup =
        [](::std::size_t cd) noexcept {
          itanium_cxa_decrement_exception_refcount(
              reinterpret_cast<void *>(cd));
        },
    // Two cxa exceptions are equivalent when they are the same exception
    // object (same catch value).
    .do_equivalent =
        [](::std::size_t cd, ::std::error_domain_singleton const *,
           ::std::size_t othercd) noexcept { return cd == othercd; },
    // The domain name is "itanium_exception", with the dynamic C++ type name
    // obtained through RTTI, e.g. "itanium_exception(std::runtime_error)". The
    // message is the what() string when the object is a std::exception.
    .do_query_information =
        [](::std::size_t cd, ::std::error_query_information query,
           ::std::error_reporter_encoding encoding, void *cookie,
           ::std::error_reporter_io_cookie_function cookfun) noexcept {

        },
    .do_to_errc = [](::std::size_t cd) noexcept -> ::std::errc {
      (void)cd;
      return ::std::errc::io_error;
    }
// Rethrow the captured legacy C++ exception. On the MSVC ABI there is no
// __cxa_rethrow_primary_exception, so this is a no-op.
#if defined(__cpp_exceptions) && !defined(_MSC_VER)
    ,
    .do_throw_dynamic_exception =
        [](::std::size_t __cd, ::std::dynamic_exception_abi __ehabi) {
          if (__ehabi != ::std::dynamic_exception_abi::platform)
            return;
#ifdef _LIBCPPABI_VERSION
          ::cxxabi::__cxa_rethrow_primary_exception(
              reinterpret_cast<void *>(__cd));
#endif
        }
#endif
};
} // namespace

extern "C" __HERBCEPTIONS_API ::std::error_domain_singleton const *
__cxa_error_domain_itanium_exception_ptr() noexcept {
  return __builtin_addressof(
      ::std::error_domains::itanium_exception_ptr_domain);
}

} // namespace std::error_domains
#endif
