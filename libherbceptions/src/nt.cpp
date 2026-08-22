//===--- nt.cpp - nt (nt_errc) error domain -------------------------------===//
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
// The NTSTATUS -> {win32, posix, message} mapping is the ntkernel error
// category table (Apache-2.0 / Boost-1.0, Niall Douglas), embedded verbatim
// from ntkernel-table.ipp. All string literals are u8"" so they are UTF-8
// regardless of the execution character set.
//
// do_equivalent handles nt<->win32 via the table's win32 column, and
// nt<->posix (or any other domain) via the posix column.
//
//===----------------------------------------------------------------------===//

#if defined(_WIN32) || defined(__CYGWIN__)

#include "domain_helpers.h"
#include "ntkernel.h"
#include "win32_common.h"

namespace std::error_domains {
namespace {
#if 0
inline constexpr ::std::io_scatter_t
nt_name_message_range(::std::error_reporter_encoding encoding,
                      ::std::size_t startpos, ::std::size_t n) noexcept {
  switch (encoding) {
  case ::std::error_reporter_encoding::utfebcdic: {
    return {&startpos["\xAD\xA3\xA3\xBD"], n}; // 4 bytes
  }
  case ::std::error_reporter_encoding::utf16: {
    return {&startpos[u"[nt]"], n * sizeof(char16_t)};
  }
  case ::std::error_reporter_encoding::utf32: {
    return {&startpos[U"[nt]"], n * sizeof(char32_t)};
  }
  default: {
    return {&startpos[u8"[nt]"], n};
  }
  }
}
#endif
using namespace __herbceptions_detail;

inline ::std::errc nt_to_errc(::std::uint_least32_t cd) noexcept {
  if (ntkernel_field const *f = find_ntstatus(cd))
    return static_cast<::std::errc>(f->posix);
  switch (cd >> 30) {
  case 0:
    return static_cast<::std::errc>(0);
  case 3:
  default:
    return ::std::errc::io_error;
  }
}

// nt <-> win32 equivalence via the table's win32 column.
inline bool nt_equivalent_win32(::std::uint_least32_t cd,
                                ::std::size_t win32cd) noexcept {
  if (ntkernel_field const *f = find_ntstatus(cd))
    return static_cast<::std::size_t>(f->win32) == win32cd;
  return false;
}

constinit ::std::error_domain_singleton __nt_error_domain{
    .do_cleanup = nullptr,
    .do_equivalent =
        [](::std::size_t cd, ::std::error_domain_singleton const *otherdomain,
           ::std::size_t othercd) noexcept {
          // nt <-> nt: identity.
          if (otherdomain == __builtin_addressof(__nt_error_domain))
            return cd == othercd;
          // nt <-> win32: use the table's win32 column.
          if (otherdomain == ::std::error_domains::__cxa_error_domain_win32())
            return nt_equivalent_win32(static_cast<::std::uint_least32_t>(cd),
                                       othercd);
          // nt <-> any other domain: compare via the POSIX errno mapping.
          return nt_to_errc(static_cast<::std::uint_least32_t>(cd)) ==
                 otherdomain->do_to_errc(othercd);
        },
    .do_query_information =
        [](::std::size_t cd, ::std::error_query_information query,
           ::std::error_reporter_encoding encoding, void *cookie,
           ::std::error_reporter_io_cookie_function cookfun) noexcept {
          ::std::error_domains::__herbceptions_detail::
              __win32_name_message_common(cd, query, encoding, cookie, cookfun,
                                          1u);
        },
    .do_to_errc = [](::std::size_t cd) noexcept -> ::std::errc {
      return ::std::errc::io_error;
    }};
} // namespace

extern "C" __HERBCEPTIONS_API ::std::error_domain_singleton const *
__cxa_error_domain_nt() noexcept {
  return __builtin_addressof(::std::error_domains::__nt_error_domain);
}

} // namespace std::error_domains

#endif // _WIN32 || __CYGWIN__
