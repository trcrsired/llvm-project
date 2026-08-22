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

#if defined(_WIN32) || defined(__CYGWIN__)

#include "domain_helpers.h"

namespace std::error_domains {
namespace {

inline constexpr ::std::io_scatter_t
com_name_message_range(::std::error_reporter_encoding encoding,
                       ::std::size_t startpos, ::std::size_t n) noexcept {
  switch (encoding) {
  case ::std::error_reporter_encoding::utfebcdic: {
    return {&startpos["\xC3\x96\x94\x93\xBD"], n}; // 5 bytes
  }
  case ::std::error_reporter_encoding::utf16: {
    return {&startpos[u"[com]"], n * sizeof(char16_t)};
  }
  case ::std::error_reporter_encoding::utf32: {
    return {&startpos[U"[com]"], n * sizeof(char32_t)};
  }
  default: {
    return {&startpos[u8"[com]"], n};
  }
  }
}

using namespace __herbceptions_detail;

constinit ::std::error_domain_singleton __com_error_domain{
    .do_cleanup = nullptr,
    .do_equivalent =
        [](::std::size_t cd, ::std::error_domain_singleton const *otherdomain,
           ::std::size_t othercd) noexcept {
          return __com_error_domain.do_to_errc(cd) ==
                 otherdomain->do_to_errc(othercd);
        },
    .do_query_information =
        [](::std::size_t, ::std::error_query_information query,
           ::std::error_reporter_encoding encoding, void *cookie,
           ::std::error_reporter_io_cookie_function cookfun) noexcept {
          query_information_pieces pieces;
          switch (query) {
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
    .do_to_errc =
        [](::std::size_t cd) noexcept { return ::std::errc::io_error; }};
} // namespace

extern "C" __HERBCEPTIONS_API ::std::error_domain_singleton const *
__cxa_error_domain_com() noexcept {
  return __builtin_addressof(::std::error_domains::__com_error_domain);
}

} // namespace std::error_domains

#endif // _WIN32 || __CYGWIN__
