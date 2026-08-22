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
#include "ntkernel.h"

namespace std::error_domains {
namespace {

inline constexpr ::std::uint_least32_t com_facility_nt_bit{0x10000000u};
inline constexpr ::std::uint_least32_t com_facility_win32{7u};
inline constexpr ::std::uint_least32_t com_hresult_code_mask{0xFFFFu};

inline constexpr ::std::uint_least32_t
com_hresult_facility(::std::uint_least32_t hr) noexcept {
  return (hr >> 16) & 0x1FFFu;
}

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

// HRESULT -> std::errc, mirroring status-code's com domain:
//   S_OK                          -> success
//   FACILITY_NT_BIT set           -> the embedded NTSTATUS via nt_errc_map
//   facility FACILITY_WIN32       -> the embedded Win32 code via win32_errc_map
//   anything else (E_FAIL & co)   -> io_error
inline ::std::errc com_to_errc(::std::uint_least32_t hr) noexcept {
  if (hr == 0) {
    return ::std::errc{};
  }
  if (hr & com_facility_nt_bit) {
    auto const nt{hr & ~com_facility_nt_bit};
    if (static_cast<::std::int_least32_t>(nt) >= 0)
      return ::std::errc{};
    switch (nt) {
#include "nt_errc_map.hpp"
    }
    return ::std::errc::io_error;
  }
  if (com_hresult_facility(hr) == com_facility_win32) {
    switch (hr & com_hresult_code_mask) {
#include "win32_errc_map.hpp"
    }
  }
  return ::std::errc::io_error;
}

constinit ::std::error_domain_singleton __com_error_domain{
    .do_cleanup = nullptr,
    .do_equivalent =
        [](::std::size_t cd, ::std::error_domain_singleton const *otherdomain,
           ::std::size_t othercd) noexcept {
          // com <-> com: identity.
          if (otherdomain == __builtin_addressof(__com_error_domain))
            return cd == othercd;
          auto const hr{static_cast<::std::uint_least32_t>(cd)};
          // com (FACILITY_NT_BIT) <-> nt: exact match on the stripped value.
          if (hr & com_facility_nt_bit) {
            if (otherdomain == ::std::error_domains::__cxa_error_domain_nt())
              return static_cast<::std::uint_least32_t>(othercd) ==
                     (hr & ~com_facility_nt_bit);
          }
          // com (FACILITY_WIN32) <-> win32: exact match on HRESULT_CODE.
          else if (com_hresult_facility(hr) == com_facility_win32) {
            if (otherdomain == ::std::error_domains::__cxa_error_domain_win32())
              return static_cast<::std::uint_least32_t>(othercd) ==
                     (hr & com_hresult_code_mask);
          }
          return __com_error_domain.do_to_errc(cd) ==
                 otherdomain->do_to_errc(othercd);
        },
    .do_query_information =
        [](::std::size_t cd, ::std::error_query_information query,
           ::std::error_reporter_encoding encoding, void *cookie,
           ::std::error_reporter_io_cookie_function cookfun) noexcept {
          // Windows DLLs provide nothing useful for plain HRESULTs
          // (E_FAIL & co), so no FormatMessage attempt is made here:
          //   FACILITY_NT_BIT codes render the ntkernel table's message for
          //   the embedded NTSTATUS; FACILITY_WIN32 codes delegate to the
          //   win32 domain, which renders the embedded Win32 code. Other
          //   facilities have no message at all.
          if (static_cast<::std::uint_least32_t>(
                  ::std::error_query_information::name_message) <
              static_cast<::std::uint_least32_t>(query)) {
            return;
          }
          ::std::io_scatter_t scatters[2];
          auto pos{scatters};
          switch (query) {
          case ::std::error_query_information::name:
            *pos = com_name_message_range(encoding, 1u, 3u);
            ++pos;
            break;
          case ::std::error_query_information::name_message:
            *pos = com_name_message_range(encoding, 0u, 5u);
            ++pos;
            [[fallthrough]];
          case ::std::error_query_information::message: {
            auto const hr{static_cast<::std::uint_least32_t>(cd)};
            if (hr & com_facility_nt_bit) {
              ntkernel_field const *f{__herbceptions_detail::find_ntstatus(
                  hr & ~com_facility_nt_bit)};
              if (f == nullptr || f->message_size == 0) {
                break;
              }
              char unsigned const *from_first{
                  reinterpret_cast<char unsigned const *>(f->message)};
              char unsigned const *from_last{from_first + f->message_size};
              alignas(char32_t) char unsigned
                  buffer[__herbceptions_detail::max_ntkernel_message_size() *
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
            if (com_hresult_facility(hr) != com_facility_win32) {
              break;
            }
            // Delegate the message to the win32 domain. The collector may be
            // invoked several times; each call appends.
            if (query == ::std::error_query_information::name_message) {
              cookfun(cookie, scatters, 1);
            } else {
              pos = scatters;
            }
            ::std::error_domains::__cxa_error_domain_win32()
                ->do_query_information(hr & com_hresult_code_mask,
                                       ::std::error_query_information::message,
                                       encoding, cookie, cookfun);
            return;
          }
          }
          cookfun(cookie, scatters, static_cast<::std::size_t>(pos - scatters));
        },
    .do_to_errc = [](::std::size_t cd) noexcept -> ::std::errc {
      return com_to_errc(static_cast<::std::uint_least32_t>(cd));
    }};
} // namespace

extern "C" __HERBCEPTIONS_API ::std::error_domain_singleton const *
__cxa_error_domain_com() noexcept {
  return __builtin_addressof(::std::error_domains::__com_error_domain);
}

} // namespace std::error_domains

#endif // _WIN32 || __CYGWIN__
