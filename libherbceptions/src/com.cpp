//===--- com_domain.cpp - com (com_errc) error domain ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the com (com_errc, HRESULT codes) error_domain singleton vtable
// and the weak __cxa_error_domain_com ABI entry point. Only built on
// _WIN32/__CYGWIN__ targets.
//
//===----------------------------------------------------------------------===//

#include "herbceptions/__details/com.h"
#include "domain_helpers.h"

#if defined(_WIN32) || defined(__CYGWIN__)

namespace std::error_domains
{
namespace
{
using namespace __herbceptions_detail;

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
    .do_query_information=[](::std::size_t, ::std::error_reporter_encoding encoding, void* cookie, ::std::error_reporter_io_cookie_function cookfun, ::std::error_query_information query) noexcept
    {
        query_information_pieces pieces;
        switch (query)
        {
        case ::std::error_query_information::name:
            pieces.add_cstr(u8"com");
            break;
        case ::std::error_query_information::message:
            break;
        case ::std::error_query_information::name_message:
            pieces.add_cstr(u8"com");
            break;
        }
        pieces.emit(encoding, cookie, cookfun);
    },
    .do_to_errc=[](::std::size_t cd) noexcept
    {
        return com_to_errc(static_cast<::std::uint_least32_t>(cd));
    }
};
} // namespace

extern "C" [[__gnu__::__weak__]]
::std::error_domain_singleton const* __cxa_error_domain_com() noexcept
{
    return __builtin_addressof(::std::error_domains::__com_error_domain);
}

} // namespace std::error_domains

#endif // _WIN32 || __CYGWIN__
