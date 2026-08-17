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

#include "domain_helpers.h"

#if defined(_WIN32) || defined(__CYGWIN__)

extern "C" void __stdcall _CxxThrowException(void*,void*);
namespace
{

constinit ::std::error_domain_singleton __msvc_exception_ptr_domain
{
    // The code is the thrown-object pointer (the __cxa catch value). When the
    // error value dies, release the reference; this destroys the exception
    // object exactly when the last reference goes away. On the MSVC ABI there
    // is no __cxa refcount, so cleanup is a no-op.
    .do_cleanup=[](::std::size_t cd) noexcept
    {
    },
    // Two cxa exceptions are equivalent when they are the same exception
    // object (same catch value).
    .do_equivalent=[](::std::size_t cd, ::std::error_domain_singleton const*, ::std::size_t othercd) noexcept
    {
        return cd == othercd;
    },
    // The domain name is "itanium_exception", with the dynamic C++ type name
    // obtained through RTTI, e.g. "itanium_exception(std::runtime_error)". The
    // message is the what() string when the object is a std::exception.
    .do_query_information=[](::std::size_t cd, ::std::error_query_information query, ::std::error_reporter_encoding encoding, void* cookie, ::std::error_reporter_io_cookie_function cookfun) noexcept
    {
    },
    .do_to_errc=[](::std::size_t cd) noexcept -> ::std::errc
    {
    },

    .do_throw_exception=[](::std::size_t cd)
    {
    }
};
}

extern "C" __HERBCEPTIONS_API
::std::error_domain_singleton const* __cxa_error_domain_msvc_exception_ptr() noexcept
{
    return __builtin_addressof(__msvc_exception_ptr_domain);
}

#endif
