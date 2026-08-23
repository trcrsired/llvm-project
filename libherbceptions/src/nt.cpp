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

namespace {

inline constexpr ::std::io_scatter_t
__nt_name_message_range(::std::error_reporter_encoding encoding,
                        ::std::size_t startpos, ::std::size_t n) noexcept {
  switch (encoding) {
  case ::std::error_reporter_encoding::utfebcdic: {
    return {&startpos["\xAD\x95\xA3\xBD"], n};
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

inline constexpr ::std::io_scatter_t
__nt_name(::std::error_reporter_encoding encoding) noexcept {
  return ::std::error_domains::__herbceptions_detail::__nt_name_message_range(
      encoding, 1u, 2u);
}

inline constexpr ::std::io_scatter_t
__nt_name_message(::std::error_reporter_encoding encoding) noexcept {
  return ::std::error_domains::__herbceptions_detail::__nt_name_message_range(
      encoding, 0u, 4u);
}

// nt -> std::errc via the generated switch table (nt_errc_map.hpp, built by
// utils/generate_win32_nt_tables.py from the ntkernel-error-category and
// status-code tables). NTSTATUS values with the severity bits clear are
// successes and map to a zero errc; anything unmapped falls back to io_error.
inline ::std::errc nt_to_errc(::std::uint_least32_t cd) noexcept {
  if (static_cast<::std::int_least32_t>(cd) >= 0)
    return ::std::errc{};
  switch (cd) {
#include "nt_errc_map.hpp"
  }
  return ::std::errc::io_error;
}

// nt <-> win32 equivalence via the table's win32 column. A zero column means
// "no Win32 equivalent" and never matches (not even ERROR_SUCCESS); codes
// absent from the table have no exact equivalent either.
inline bool nt_equivalent_win32(::std::uint_least32_t cd,
                                ::std::size_t win32cd) noexcept {
  using namespace ::std::error_domains::__herbceptions_detail;
  if (auto f = find_ntstatus(cd))
    return f->win32 != 0 && static_cast<::std::size_t>(f->win32) == win32cd;
  return false;
}

constinit ::std::error_domain_singleton nt_error_domain{
    .do_cleanup = nullptr,
    .do_equivalent =
        [](::std::size_t cd, ::std::error_domain_singleton const *otherdomain,
           ::std::size_t othercd) noexcept {
          // nt <-> nt: identity.
          if (otherdomain == __builtin_addressof(nt_error_domain))
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
          // The message is the ntkernel table's descriptive string (u8"",
          // UTF-8 regardless of the execution character set). Windows DLLs
          // (FormatMessageW on ntdll) provide nothing useful for NTSTATUS,
          // so no fallback is attempted; codes absent from the table have
          // no message at all.
          if (static_cast<::std::uint_least32_t>(
                  ::std::error_query_information::name_message) <
              static_cast<::std::uint_least32_t>(query)) {
            return;
          }
          ::std::io_scatter_t scatters[2];
          auto pos{scatters};
          switch (query) {
          case ::std::error_query_information::name: {
            *pos = __nt_name(encoding);
            ++pos;
            break;
          }
          case ::std::error_query_information::name_message:
            *pos = __nt_name_message(encoding);
            ++pos;
            [[fallthrough]];
          case ::std::error_query_information::message: {
            auto f{::std::error_domains::__herbceptions_detail::find_ntstatus(
                static_cast<::std::uint_least32_t>(cd))};
            if (f == nullptr || f->message_size == 0) {
              break;
            }
            char unsigned const *from_first{
                reinterpret_cast<char unsigned const *>(f->message)};
            char unsigned const *from_last{from_first + f->message_size};
            alignas(char32_t) char unsigned
                buffer[::std::error_domains::__herbceptions_detail::
                           max_ntkernel_message_size() *
                       sizeof(char32_t)];
            switch (encoding) {
            case ::std::error_reporter_encoding::utfebcdic: {
              auto dest{::std::error_domains::__herbceptions_detail::
                            __write_ebcdic_with_ascii_only_range(
                                from_first, from_last, buffer)};
              *pos = {buffer, static_cast<::std::size_t>(dest - buffer)};
              break;
            }
            case ::std::error_reporter_encoding::utf16: {
              using __char16_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
                  [[__gnu__::__may_alias__]]
#endif
                  = char16_t *;
              auto dest{
                  ::std::error_domains::__herbceptions_detail::
                      __write_with_ascii_only_range(
                          from_first, from_last,
                          reinterpret_cast<__char16_may_alias_ptr>(buffer))};
              *pos = {buffer,
                      static_cast<::std::size_t>(
                          reinterpret_cast<char unsigned *>(dest) - buffer)};
              break;
            }
            case ::std::error_reporter_encoding::utf32: {
              using __char32_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
                  [[__gnu__::__may_alias__]]
#endif
                  = char32_t *;
              auto dest{
                  ::std::error_domains::__herbceptions_detail::
                      __write_with_ascii_only_range(
                          from_first, from_last,
                          reinterpret_cast<__char32_may_alias_ptr>(buffer))};
              *pos = {buffer,
                      static_cast<::std::size_t>(
                          reinterpret_cast<char unsigned *>(dest) - buffer)};
              break;
            }
            default: {
              *pos = {from_first,
                      static_cast<::std::size_t>(from_last - from_first)};
              break;
            }
            }
            ++pos;
            break;
          }
          }
          cookfun(cookie, scatters, static_cast<::std::size_t>(pos - scatters));
        },
    .do_to_errc = [](::std::size_t cd) noexcept -> ::std::errc {
      return nt_to_errc(static_cast<::std::uint_least32_t>(cd));
    }};
} // namespace

extern "C" __HERBCEPTIONS_API ::std::error_domain_singleton const *
__cxa_error_domain_nt() noexcept {
  return __builtin_addressof(nt_error_domain);
}

#endif // _WIN32 || __CYGWIN__
