//===--- wine.cpp - wine (wine_errc) error domain -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the wine (wine_errc) error_domain singleton vtable and the weak
// __cxa_error_domain_wine ABI entry point. Only built on _WIN32/__CYGWIN__
// targets. wine_errc uses Wine's own UNIX errno values (Linux-kernel style
// numbering); see __wine_unix_errno.h in fast_io.
//
//===----------------------------------------------------------------------===//

#include "herbceptions/__details/wine.h"
#include "domain_helpers.h"

#if defined(_WIN32) || defined(__CYGWIN__)

namespace std::error_domains
{
namespace
{
using namespace __herbceptions_detail;

constinit ::std::error_domain_singleton __wine_error_domain
{
    .do_cleanup=nullptr,
    .do_equivalent=[](::std::size_t cd, ::std::error_domain_singleton const* otherdomain, ::std::size_t othercd) noexcept
    {
        return __wine_error_domain.do_to_errc(cd) == otherdomain->do_to_errc(othercd);
    },
    .do_query_information=[](::std::size_t cd, ::std::error_query_information query, ::std::error_reporter_encoding encoding, void* cookie, ::std::error_reporter_io_cookie_function cookfun) noexcept
    {
        // strerror is not used (not thread-safe); use the static errc table.
        query_information_pieces pieces;
        switch (query)
        {
        case ::std::error_query_information::name:
            pieces.add_cstr(u8"wine");
            break;
        case ::std::error_query_information::message:
            pieces.add_cstr(errc_message(static_cast<int>(cd)));
            break;
        case ::std::error_query_information::name_message:
            pieces.add_cstr(u8"wine");
            pieces.add_cstr(errc_message(static_cast<int>(cd)));
            break;
        }
        pieces.emit(encoding, cookie, cookfun);
    },
    .do_to_errc=[](::std::size_t cd) noexcept
    {
        return static_cast<::std::errc>(cd);
    }
};
} // namespace

extern "C" [[__gnu__::__weak__]]
::std::error_domain_singleton const* __cxa_error_domain_wine() noexcept
{
    return __builtin_addressof(::std::error_domains::__wine_error_domain);
}

} // namespace std::error_domains

#endif // _WIN32 || __CYGWIN__
