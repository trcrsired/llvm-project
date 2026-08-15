//===--- error_domains.cpp - Herbception error-domain runtime -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the error_domain_singleton vtables for the standard
// error domains. The posix (std::errc) and cxa_exception_code domains are
// always built; the win32/nt/com/wine domains are built only when _WIN32 or
// __CYGWIN__ is defined.
//
// Every domain provides the error_domain_singleton vtable so std::error
// values from different domains interoperate: do_equivalent converts both
// sides to std::errc (the POSIX canonical form); do_message and do_name
// produce human-readable text without allocating (IO-cookie model).
//
//===----------------------------------------------------------------------===//

#include "herbceptions/error_domains.h"
#include "herbceptions/cxa_exception_code.h"

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

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

void write_text(::std::error_reporter_encoding encoding, void* cookie,
                ::std::error_reporter_io_cookie_function cookfun,
                void const* base, ::std::size_t len) noexcept
{
    ::std::io_scatter_t v{base, len};
    cookfun(encoding, cookie, __builtin_addressof(v), 1u);
}

// Write a fixed ASCII string in the requested encoding. The strings used for
// names/messages here are ASCII-only, so they are valid in every encoding.
void write_ascii(::std::error_reporter_encoding encoding, void* cookie,
                 ::std::error_reporter_io_cookie_function cookfun,
                 char const* s) noexcept
{
    write_text(encoding, cookie, cookfun, s, __builtin_strlen(s));
}

// ---------------------------------------------------------------------------
// posix — std::errc
// ---------------------------------------------------------------------------
// Static, thread-safe errno message table. strerror is not used because it is
// not guaranteed thread-safe; the messages here match the POSIX strerror text
// for the standard errno values.
struct errc_message_entry
{
    int code;
    char const* message;
};

constexpr errc_message_entry errc_messages[] = {
    {0, "Success"},
    {1, "Operation not permitted"},
    {2, "No such file or directory"},
    {3, "No such process"},
    {4, "Interrupted system call"},
    {5, "Input/output error"},
    {6, "No such device or address"},
    {7, "Argument list too long"},
    {8, "Exec format error"},
    {9, "Bad file descriptor"},
    {10, "No child processes"},
    {11, "Resource temporarily unavailable"},
    {12, "Cannot allocate memory"},
    {13, "Permission denied"},
    {14, "Bad address"},
    {16, "Device or resource busy"},
    {17, "File exists"},
    {18, "Invalid cross-device link"},
    {19, "No such device"},
    {20, "Not a directory"},
    {21, "Is a directory"},
    {22, "Invalid argument"},
    {23, "Too many open files in system"},
    {24, "Too many open files"},
    {25, "Inappropriate ioctl for device"},
    {26, "Text file busy"},
    {27, "File too large"},
    {28, "No space left on device"},
    {29, "Illegal seek"},
    {30, "Read-only file system"},
    {31, "Too many links"},
    {32, "Broken pipe"},
    {33, "Numerical argument out of domain"},
    {34, "Numerical result out of range"},
    {35, "Resource deadlock avoided"},
    {36, "File name too long"},
    {37, "No locks available"},
    {38, "Function not implemented"},
    {39, "Directory not empty"},
    {40, "Too many levels of symbolic links"},
    {42, "No message of desired type"},
    {43, "Identifier removed"},
    {61, "No data available"},
    {67, "Link has been severed"},
    {71, "Protocol error"},
    {74, "Bad message"},
    {75, "Value too large for defined data type"},
    {84, "Invalid or incomplete multibyte or wide character"},
    {88, "Socket operation on non-socket"},
    {89, "Destination address required"},
    {90, "Message too long"},
    {91, "Protocol wrong type for socket"},
    {92, "Protocol not available"},
    {93, "Protocol not supported"},
    {95, "Operation not supported"},
    {97, "Address family not supported by protocol"},
    {98, "Address already in use"},
    {99, "Cannot assign requested address"},
    {100, "Network is down"},
    {101, "Network is unreachable"},
    {102, "Network dropped connection on reset"},
    {103, "Software caused connection abort"},
    {104, "Connection reset by peer"},
    {105, "No buffer space available"},
    {107, "Transport endpoint is not connected"},
    {110, "Connection timed out"},
    {111, "Connection refused"},
    {113, "Host is unreachable"},
    {114, "Operation already in progress"},
    {115, "Operation now in progress"},
    {125, "Operation canceled"},
    {130, "Owner died"},
    {131, "State not recoverable"},
};

char const* errc_message(int code) noexcept
{
    for (const errc_message_entry& e : errc_messages)
        if (e.code == code)
            return e.message;
    return "Unknown error";
}

constinit ::std::error_domain_singleton __posix_error_domain
{
    .do_cleanup=nullptr, // errno values need no cleanup
    .do_equivalent=[](::std::size_t cd, ::std::error_domain_singleton const* otherdomain, ::std::size_t othercd) noexcept
    {
        return __posix_error_domain.do_to_errc(cd) == otherdomain->do_to_errc(othercd);
    },
    .do_name=[](::std::size_t, ::std::error_reporter_encoding encoding, void* cookie, ::std::error_reporter_io_cookie_function cookfun) noexcept
    {
        write_ascii(encoding, cookie, cookfun, "posix");
    },
    .do_message=[](::std::size_t cd, ::std::error_reporter_encoding encoding, void* cookie, ::std::error_reporter_io_cookie_function cookfun) noexcept
    {
        write_ascii(encoding, cookie, cookfun, errc_message(static_cast<int>(cd)));
    },
    .do_to_errc=[](::std::size_t cd) noexcept
    {
        return static_cast<::std::errc>(cd);
    }
};

// ---------------------------------------------------------------------------
// cxa_exception_code — legacy C++ exception carrier. See cxa_exception_code.h
// for the type; the vtable lives here.
//
// The __cxa_* ABI (allocate/free/refcount/rethrow) exists only on Itanium-ABI
// targets (Linux, macOS, Cygwin, MinGW). The MSVC ABI (indicated by _MSC_VER)
// uses __CxxFrameHandler3/_CxxThrowException instead, so the cxa_exception_code
// vtable degrades gracefully there (no refcount/rethrow support).
// ---------------------------------------------------------------------------
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
        write_ascii(encoding, cookie, cookfun, "cxa_exception");
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

#if defined(_WIN32) || defined(__CYGWIN__)

// ---------------------------------------------------------------------------
// win32 — win32_errc
// ---------------------------------------------------------------------------
::std::errc win32_to_errc(::std::uint_least32_t cd) noexcept
{
    switch (static_cast<::std::win32_errc>(cd))
    {
    case ::std::win32_errc::success:
        return static_cast<::std::errc>(0);
    case ::std::win32_errc::access_denied:
        return ::std::errc::permission_denied;
    case ::std::win32_errc::file_not_found:
    case ::std::win32_errc::path_not_found:
        return ::std::errc::no_such_file_or_directory;
    case ::std::win32_errc::invalid_function:
    case ::std::win32_errc::invalid_parameter:
        return ::std::errc::invalid_argument;
    case ::std::win32_errc::not_enough_memory:
    case ::std::win32_errc::out_of_memory:
    case ::std::win32_errc::insufficient_buffer:
        return ::std::errc::not_enough_memory;
    case ::std::win32_errc::file_exists:
    case ::std::win32_errc::already_exists:
        return ::std::errc::file_exists;
    case ::std::win32_errc::broken_pipe:
        return ::std::errc::broken_pipe;
    case ::std::win32_errc::too_many_open_files:
        return ::std::errc::too_many_files_open;
    case ::std::win32_errc::disk_full:
    case ::std::win32_errc::handle_disk_full:
        return ::std::errc::no_space_on_device;
    case ::std::win32_errc::operation_aborted:
    case ::std::win32_errc::canceled:
        return ::std::errc::operation_canceled;
    case ::std::win32_errc::sharing_violation:
    case ::std::win32_errc::lock_violation:
        return ::std::errc::device_or_resource_busy;
    case ::std::win32_errc::not_supported:
        return ::std::errc::not_supported;
    default:
        return ::std::errc::io_error;
    }
}

constinit ::std::error_domain_singleton __win32_error_domain
{
    .do_cleanup=nullptr,
    .do_equivalent=[](::std::size_t cd, ::std::error_domain_singleton const* otherdomain, ::std::size_t othercd) noexcept
    {
        return win32_to_errc(static_cast<::std::uint_least32_t>(cd)) ==
               otherdomain->do_to_errc(othercd);
    },
    .do_name=[](::std::size_t, ::std::error_reporter_encoding encoding, void* cookie, ::std::error_reporter_io_cookie_function cookfun) noexcept
    {
        write_ascii(encoding, cookie, cookfun, "win32");
    },
    .do_message=[](::std::size_t, ::std::error_reporter_encoding encoding, void* cookie, ::std::error_reporter_io_cookie_function cookfun) noexcept
    {
        write_ascii(encoding, cookie, cookfun, "");
    },
    .do_to_errc=[](::std::size_t cd) noexcept
    {
        return win32_to_errc(static_cast<::std::uint_least32_t>(cd));
    }
};

// ---------------------------------------------------------------------------
// nt — nt_errc (NTSTATUS)
// ---------------------------------------------------------------------------
::std::errc nt_to_errc(::std::uint_least32_t cd) noexcept
{
    switch (static_cast<::std::nt_errc>(cd))
    {
    case ::std::nt_errc::success:
        return static_cast<::std::errc>(0);
    case ::std::nt_errc::access_denied:
    case ::std::nt_errc::privilege_not_held:
        return ::std::errc::permission_denied;
    case ::std::nt_errc::no_such_file:
    case ::std::nt_errc::object_name_not_found:
    case ::std::nt_errc::object_path_not_found:
        return ::std::errc::no_such_file_or_directory;
    case ::std::nt_errc::invalid_parameter:
    case ::std::nt_errc::invalid_parameter_1:
    case ::std::nt_errc::invalid_parameter_2:
        return ::std::errc::invalid_argument;
    case ::std::nt_errc::insufficient_resources:
    case ::std::nt_errc::buffer_too_small:
        return ::std::errc::not_enough_memory;
    case ::std::nt_errc::disk_full:
        return ::std::errc::no_space_on_device;
    case ::std::nt_errc::share_violation:
    case ::std::nt_errc::file_locked:
        return ::std::errc::device_or_resource_busy;
    case ::std::nt_errc::not_implemented:
        return ::std::errc::function_not_supported;
    case ::std::nt_errc::not_a_directory:
        return ::std::errc::not_a_directory;
    case ::std::nt_errc::directory_not_empty:
        return ::std::errc::directory_not_empty;
    default:
        return ::std::errc::io_error;
    }
}

constinit ::std::error_domain_singleton __nt_error_domain
{
    .do_cleanup=nullptr,
    .do_equivalent=[](::std::size_t cd, ::std::error_domain_singleton const* otherdomain, ::std::size_t othercd) noexcept
    {
        return nt_to_errc(static_cast<::std::uint_least32_t>(cd)) ==
               otherdomain->do_to_errc(othercd);
    },
    .do_name=[](::std::size_t, ::std::error_reporter_encoding encoding, void* cookie, ::std::error_reporter_io_cookie_function cookfun) noexcept
    {
        write_ascii(encoding, cookie, cookfun, "nt");
    },
    .do_message=[](::std::size_t, ::std::error_reporter_encoding encoding, void* cookie, ::std::error_reporter_io_cookie_function cookfun) noexcept
    {
        write_ascii(encoding, cookie, cookfun, "");
    },
    .do_to_errc=[](::std::size_t cd) noexcept
    {
        return nt_to_errc(static_cast<::std::uint_least32_t>(cd));
    }
};

// ---------------------------------------------------------------------------
// com — com_errc (HRESULT)
// ---------------------------------------------------------------------------
::std::errc com_to_errc(::std::uint_least32_t cd) noexcept
{
    switch (static_cast<::std::com_errc>(cd))
    {
    case ::std::com_errc::ok:
        return static_cast<::std::errc>(0);
    case ::std::com_errc::accessdenied:
        return ::std::errc::permission_denied;
    case ::std::com_errc::outofmemory:
        return ::std::errc::not_enough_memory;
    case ::std::com_errc::invalidarg:
        return ::std::errc::invalid_argument;
    case ::std::com_errc::notimpl:
    case ::std::com_errc::fail:
        return ::std::errc::io_error;
    case ::std::com_errc::cancelled:
        return ::std::errc::operation_canceled;
    case ::std::com_errc::notfound:
        return ::std::errc::no_such_file_or_directory;
    case ::std::com_errc::alreadyexists:
        return ::std::errc::file_exists;
    default:
        return ::std::errc::io_error;
    }
}

constinit ::std::error_domain_singleton __com_error_domain
{
    .do_cleanup=nullptr,
    .do_equivalent=[](::std::size_t cd, ::std::error_domain_singleton const* otherdomain, ::std::size_t othercd) noexcept
    {
        return com_to_errc(static_cast<::std::uint_least32_t>(cd)) ==
               otherdomain->do_to_errc(othercd);
    },
    .do_name=[](::std::size_t, ::std::error_reporter_encoding encoding, void* cookie, ::std::error_reporter_io_cookie_function cookfun) noexcept
    {
        write_ascii(encoding, cookie, cookfun, "com");
    },
    .do_message=[](::std::size_t, ::std::error_reporter_encoding encoding, void* cookie, ::std::error_reporter_io_cookie_function cookfun) noexcept
    {
        write_ascii(encoding, cookie, cookfun, "");
    },
    .do_to_errc=[](::std::size_t cd) noexcept
    {
        return com_to_errc(static_cast<::std::uint_least32_t>(cd));
    }
};

// ---------------------------------------------------------------------------
// wine — wine_errc (Wine's UNIX errno values; see __wine_unix_errno.h)
// ---------------------------------------------------------------------------
constinit ::std::error_domain_singleton __wine_error_domain
{
    .do_cleanup=nullptr,
    .do_equivalent=[](::std::size_t cd, ::std::error_domain_singleton const* otherdomain, ::std::size_t othercd) noexcept
    {
        return __wine_error_domain.do_to_errc(cd) == otherdomain->do_to_errc(othercd);
    },
    .do_name=[](::std::size_t, ::std::error_reporter_encoding encoding, void* cookie, ::std::error_reporter_io_cookie_function cookfun) noexcept
    {
        write_ascii(encoding, cookie, cookfun, "wine");
    },
    .do_message=[](::std::size_t cd, ::std::error_reporter_encoding encoding, void* cookie, ::std::error_reporter_io_cookie_function cookfun) noexcept
    {
        // strerror is not used (not thread-safe); use the static errc table.
        write_ascii(encoding, cookie, cookfun, errc_message(static_cast<int>(cd)));
    },
    .do_to_errc=[](::std::size_t cd) noexcept
    {
        return static_cast<::std::errc>(cd);
    }
};

#endif // _WIN32 || __CYGWIN__

} // namespace

// ---------------------------------------------------------------------------
// Weak ABI entry points. Each is overridable so a freestanding environment
// can provide its own implementation without relinking the library.
// ---------------------------------------------------------------------------
extern "C" [[__gnu__::__weak__]]
::std::error_domain_singleton const* __cxa_error_domain_posix() noexcept
{
    return __builtin_addressof(::std::error_domains::__posix_error_domain);
}

extern "C" [[__gnu__::__weak__]]
::std::error_domain_singleton const* __cxa_error_domain_cxa_exception_code() noexcept
{
    return __builtin_addressof(::std::error_domains::__cxa_exception_error_domain);
}

#if defined(_WIN32) || defined(__CYGWIN__)
extern "C" [[__gnu__::__weak__]]
::std::error_domain_singleton const* __cxa_error_domain_win32() noexcept
{
    return __builtin_addressof(::std::error_domains::__win32_error_domain);
}

extern "C" [[__gnu__::__weak__]]
::std::error_domain_singleton const* __cxa_error_domain_nt() noexcept
{
    return __builtin_addressof(::std::error_domains::__nt_error_domain);
}

extern "C" [[__gnu__::__weak__]]
::std::error_domain_singleton const* __cxa_error_domain_com() noexcept
{
    return __builtin_addressof(::std::error_domains::__com_error_domain);
}

extern "C" [[__gnu__::__weak__]]
::std::error_domain_singleton const* __cxa_error_domain_wine() noexcept
{
    return __builtin_addressof(::std::error_domains::__wine_error_domain);
}
#endif // _WIN32 || __CYGWIN__

} // namespace std::error_domains
