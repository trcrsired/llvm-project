//===--- win32_domain.cpp - win32 (win32_errc) error domain ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the win32 (win32_errc, Win32 GetLastError codes) error_domain
// singleton vtable and the weak __cxa_error_domain_win32 ABI entry point.
// Only built on _WIN32/__CYGWIN__ targets.
//
//===----------------------------------------------------------------------===//

#include "herbceptions/__details/win32.h"
#include "domain_helpers.h"

#if defined(_WIN32) || defined(__CYGWIN__)

namespace std::error_domains
{
namespace
{
using namespace __herbceptions_detail;

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
} // namespace

extern "C" [[__gnu__::__weak__]]
::std::error_domain_singleton const* __cxa_error_domain_win32() noexcept
{
    return __builtin_addressof(::std::error_domains::__win32_error_domain);
}

} // namespace std::error_domains

#endif // _WIN32 || __CYGWIN__
