//===--- cxa_exception_code_domain.cpp - cxa_exception_code domain -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the cxa_exception_code error_domain_singleton vtable and the
// weak __cxa_error_domain_cxa_exception_code ABI entry point. Available on
// all platforms. The code is the thrown-object pointer (the __cxa catch
// value); the vtable releases it on cleanup and rethrows it via the ABI.
//
// The __cxa_* ABI (allocate/free/refcount/rethrow) exists only on Itanium-ABI
// targets (Linux, macOS, Cygwin, MinGW). The MSVC ABI (indicated by _MSC_VER)
// uses __CxxFrameHandler3/_CxxThrowException instead, so the vtable degrades
// gracefully there (no refcount/rethrow support).
//
//===----------------------------------------------------------------------===//

#include "herbceptions/__details/cxa_exception_code.h"
#include "domain_helpers.h"

#if defined(__cpp_exceptions) && !defined(_MSC_VER) && \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 1
#include <exception>
#include <new>
#include <stdexcept>
#include <system_error>
#include <typeinfo>
#endif

namespace std::error_domains
{
namespace
{
using namespace __herbceptions_detail;

#if defined(__cpp_exceptions) && !defined(_MSC_VER)
// These are libc++abi symbols; libstdc++ hides them but libc++abi exports
// them. Declare them weak so the shared library links even when the C++ ABI
// library does not export them (they resolve at runtime when provided).
extern "C" __attribute__((weak)) void __cxa_decrement_exception_refcount(void*) noexcept;
extern "C" __attribute__((weak)) void __cxa_increment_exception_refcount(void*) noexcept;
extern "C" __attribute__((weak)) void __cxa_rethrow_primary_exception(void*) noexcept;

// The __cxa_exception header precedes the thrown object in the same
// allocation ([header][thrown object]). Given the thrown-object pointer (the
// code / catch value), the header is at thrown_object - 1 (in units of the
// FULL __cxa_exception) and holds the std::type_info* of the dynamic type.
// This layout mirrors libc++abi's __cxa_exception on LP64 Itanium.
struct __cxa_exception_layout
{
    void* reserve;                              //  0
    ::std::size_t referenceCount;               //  8
    ::std::type_info* exceptionType;            // 16
    void (*exceptionDestructor)(void*);         // 24
    void (*unexpectedHandler)();                // 32
    void (*terminateHandler)();                 // 40
    void* nextException;                        // 48
    int handlerCount;                           // 56
    int handlerSwitchValue;                     // 60
    unsigned char const* actionRecord;          // 64
    unsigned char const* languageSpecificData;  // 72
    void* catchTemp;                            // 80
    void* adjustedPtr;                          // 88
    struct _Unwind_Exception
    {
        unsigned long long exception_class;     // 96
        void (*exception_cleanup)(int, void*);  // 104
        unsigned long private_1;                // 112
        unsigned long private_2;                // 120
    } unwindHeader;                             // sizeof = 128
};
static_assert(sizeof(__cxa_exception_layout) == 128,
              "unexpected __cxa_exception layout");

constexpr ::std::type_info* cxa_type_info_of(::std::size_t cd) noexcept
{
    if (cd == 0)
        return nullptr;
    __cxa_exception_layout const* header =
        reinterpret_cast<__cxa_exception_layout const*>(cd) - 1;
    return header->exceptionType;
}

// Itanium ABI catch-matching primitive (the same one the personality routine
// uses): returns whether a handler of type T could catch the thrown object,
// storing the adjusted pointer in __obj.
template <typename T>
bool is_catchable_as(::std::size_t cd, void*& __obj) noexcept
{
    ::std::type_info const* thrown = cxa_type_info_of(cd);
    if (!thrown)
        return false;
    void* adjusted = reinterpret_cast<void*>(cd);
    if (thrown->__do_catch(&typeid(T), &adjusted, 0u))
    {
        __obj = adjusted;
        return true;
    }
    return false;
}

::std::errc errc_of(::std::error_code const& __ec) noexcept
{
    if (__ec.category() == ::std::generic_category())
        return static_cast<::std::errc>(__ec.value());
    return ::std::errc::io_error;
}
#endif // __cpp_exceptions && !_MSC_VER

constinit ::std::error_domain_singleton __cxa_exception_error_domain
{
    // The code is the thrown-object pointer (the __cxa catch value). When the
    // error value dies, release the reference; this destroys the exception
    // object exactly when the last reference goes away. On the MSVC ABI there
    // is no __cxa refcount, so cleanup is a no-op.
    .do_cleanup=[](::std::size_t cd) noexcept
    {
#if defined(__cpp_exceptions) && !defined(_MSC_VER)
        __cxa_decrement_exception_refcount(reinterpret_cast<void*>(cd));
#endif
    },
    // Two cxa exceptions are equivalent when they are the same exception
    // object (same catch value).
    .do_equivalent=[](::std::size_t cd, ::std::error_domain_singleton const*, ::std::size_t othercd) noexcept
    {
        return cd == othercd;
    },
    // The domain name is "cxa_exception", with the dynamic C++ type name
    // obtained through RTTI, e.g. "cxa_exception(std::runtime_error)".
    .do_name=[](::std::size_t cd, ::std::error_reporter_encoding encoding, void* cookie, ::std::error_reporter_io_cookie_function cookfun) noexcept
    {
        ::std::io_scatter_t v[3];
        write_ascii(encoding, cookie, cookfun, u8"cxa_exception");
        v[0].base = "(";
        v[0].len = 1;
#if defined(__cpp_exceptions) && !defined(_MSC_VER)
        ::std::type_info const* thrown = cxa_type_info_of(cd);
        v[1].base = thrown ? static_cast<void const*>(thrown->name())
                           : static_cast<void const*>("?");
        v[1].len = thrown ? __builtin_strlen(thrown->name()) : 1;
#else
        (void)cd;
        v[1].base = "?";
        v[1].len = 1;
#endif
        v[2].base = ")";
        v[2].len = 1;
        cookfun(encoding, cookie, v, 3u);
    },
    // The message is the what() string when the object is a std::exception.
    .do_message=[](::std::size_t cd, ::std::error_reporter_encoding encoding, void* cookie, ::std::error_reporter_io_cookie_function cookfun) noexcept
    {
#if defined(__cpp_exceptions) && !defined(_MSC_VER)
        void* obj = nullptr;
        if (is_catchable_as<::std::exception>(cd, obj))
        {
            ::std::exception const* e = static_cast<::std::exception const*>(obj);
            char const* what = e->what();
            write_text(encoding, cookie, cookfun, what, __builtin_strlen(what));
        }
#else
        (void)cd;
        (void)encoding;
        (void)cookie;
        (void)cookfun;
#endif
    },
    .do_to_errc=[](::std::size_t cd) noexcept -> ::std::errc
    {
#if defined(__cpp_exceptions) && !defined(_MSC_VER)
        void* obj = nullptr;
        if (is_catchable_as<::std::system_error>(cd, obj))
            return static_cast<::std::errc>(
                errc_of(static_cast<::std::system_error const*>(obj)->code()));
        if (is_catchable_as<::std::bad_alloc>(cd, obj))
            return ::std::errc::not_enough_memory;
        if (is_catchable_as<::std::length_error>(cd, obj))
            return ::std::errc::value_too_large;
        if (is_catchable_as<::std::out_of_range>(cd, obj))
            return ::std::errc::result_out_of_range;
        if (is_catchable_as<::std::overflow_error>(cd, obj))
            return ::std::errc::value_too_large;
        if (is_catchable_as<::std::underflow_error>(cd, obj))
            return ::std::errc::result_out_of_range;
        if (is_catchable_as<::std::domain_error>(cd, obj))
            return ::std::errc::argument_out_of_domain;
        if (is_catchable_as<::std::invalid_argument>(cd, obj))
            return ::std::errc::invalid_argument;
        return ::std::errc::io_error;
#else
        (void)cd;
        return ::std::errc::io_error;
#endif
    },
    // Rethrow the captured legacy C++ exception. On the MSVC ABI there is no
    // __cxa_rethrow_primary_exception, so this is a no-op.
#if defined(__cpp_exceptions) && !defined(_MSC_VER)
    .do_throw_cxa_exception=[](::std::size_t cd, ::std::cxa_exception_abi abi)
    {
        if (abi == ::std::cxa_exception_abi::itanium)
            __cxa_rethrow_primary_exception(reinterpret_cast<void*>(cd));
    }
#endif
};
} // namespace

extern "C" [[__gnu__::__weak__]]
::std::error_domain_singleton const* __cxa_error_domain_cxa_exception_code() noexcept
{
    return __builtin_addressof(::std::error_domains::__cxa_exception_error_domain);
}

} // namespace std::error_domains
