//===--- posix_domain.cpp - posix (std::errc) error domain ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the posix (std::errc) error_domain_singleton vtable and the weak
// __cxa_error_domain_posix ABI entry point. Available on all platforms.
//
//===----------------------------------------------------------------------===//

#include "herbceptions/__details/posix.h"
#include "domain_helpers.h"

namespace std::error_domains
{
namespace
{
using namespace __herbceptions_detail;

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
} // namespace

extern "C" [[__gnu__::__weak__]]
::std::error_domain_singleton const* __cxa_error_domain_posix() noexcept
{
    return __builtin_addressof(::std::error_domains::__posix_error_domain);
}

} // namespace std::error_domains
