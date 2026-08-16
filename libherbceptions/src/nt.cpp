//===--- nt_domain.cpp - nt (nt_errc) error domain ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the nt (nt_errc, NTSTATUS STATUS_* codes) error_domain singleton
// vtable and the weak __cxa_error_domain_nt ABI entry point. Only built on
// _WIN32/__CYGWIN__ targets.
//
//===----------------------------------------------------------------------===//

#include "herbceptions/__details/nt.h"
#include "domain_helpers.h"

#if defined(_WIN32) || defined(__CYGWIN__)

namespace std::error_domains
{
namespace
{
using namespace __herbceptions_detail;

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
} // namespace

extern "C" [[__gnu__::__weak__]]
::std::error_domain_singleton const* __cxa_error_domain_nt() noexcept
{
    return __builtin_addressof(::std::error_domains::__nt_error_domain);
}

} // namespace std::error_domains

#endif // _WIN32 || __CYGWIN__
