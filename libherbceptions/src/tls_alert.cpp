//===--- tls_alert.cpp - tls_alert error domain -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the tls_alert error_domain_singleton vtable and the
// __cxa_error_domain_tls ABI entry point. Available on all platforms.
// Deliberately self-contained — it does NOT include
// simple_query_information_common.h, so linking the domain does not drag
// the shared table machinery (and every other domain's strings) into the
// binary. Name/message strings come from src/tls_alert_table.hpp.
//
// The packed code is (level << 8) | description, but only the description
// byte is reported numerically: it is the IANA "TLS Alerts" wire code, and
// RFC 9846 forbids receivers from changing behaviour based on the level of
// an error alert.
//
//===----------------------------------------------------------------------===//

#include "libherbceptions.h"
#include <herbceptions/__details/tls.h>

namespace {

using ::std::error_domains::__herbceptions_detail::
    __format_decimal_pair_value_full_with_bracket;
using ::std::error_domains::__herbceptions_detail::__messages_disabled;
using ::std::error_domains::__herbceptions_detail::__tsc;
using ::std::error_domains::__herbceptions_detail::
    __write_with_ascii_only_range;
#ifdef __LIBHERBCEPTIONS_ENABLE_EBCDIC
using ::std::error_domains::__herbceptions_detail::
    __write_ebcdic_with_ascii_only_range;
#endif

inline constexpr ::std::io_scatter_t
__tls_name_message_range(::std::error_reporter_encoding encoding,
                         ::std::size_t startpos, ::std::size_t n) noexcept {
  switch (encoding) {
#ifdef __LIBHERBCEPTIONS_ENABLE_EBCDIC
  case ::std::error_reporter_encoding::utfebcdic: {
    return {&startpos["\xAD\xA3\x93\xA2\xBD"], n};
  }
#endif
  case ::std::error_reporter_encoding::utf16: {
    return {&startpos[u"[tls]"], n * sizeof(char16_t)};
  }
  case ::std::error_reporter_encoding::utf32: {
    return {&startpos[U"[tls]"], n * sizeof(char32_t)};
  }
  default: {
    return {&startpos[u8"[tls]"], n};
  }
  }
}

inline constexpr ::std::io_scatter_t
__to_u8scatter_from_tls_alert(::std::uint_least32_t __eno) noexcept {
  switch (__eno) {
#include "tls_alert_table.hpp"
  }
}

/* the level literal and separator: "Warning: " / "Fatal: " /
   "Unknown: " -- the RFC 9846 Section 6 level names */
inline constexpr ::std::io_scatter_t
__tls_level_literal(::std::uint_least32_t __level) noexcept {
  switch (static_cast<::std::tls_alert::alert_level>(
      static_cast<::std::uint_least8_t>(__level))) {
  case ::std::tls_alert::alert_level::warning:
    return __tsc(u8"Warning: ");
  case ::std::tls_alert::alert_level::fatal:
    return __tsc(u8"Fatal: ");
  default:
    return __tsc(u8"Unknown: ");
  }
}

inline void __tls_query_information(
    ::std::size_t cd, ::std::error_query_information query,
    ::std::error_reporter_encoding encoding, void *cookie,
    ::std::error_reporter_io_cookie_function cookfun) noexcept {
  if (static_cast<::std::uint_least32_t>(
          ::std::error_query_information::name_message) <
      static_cast<::std::uint_least32_t>(query)) {
    return;
  }
  auto const pck{static_cast<::std::uint_least32_t>(cd)};
  auto const level{(pck >> 8) & 0xffu};
  auto const desc{pck & 0xffu};
  ::std::io_scatter_t __scatters[2];
  ::std::size_t __scatterlen{};
  /*
  The whole "(<level>:<desc>)<Literal>: <message>" tail is composed as
  ASCII in __ascii and transcoded once into the requested encoding
  through __buffer, so a message/name_message reply emits at most two
  scatters. With __messages_disabled the compose stops right after the
  "(<level>:<desc>)" part. The scatter referencing __buffer is consumed
  by the cookfun call at the end of this function, so both buffers must
  be declared at function scope.
  */
  char unsigned __ascii[8 + 7 + 2 + TLS_ALERT_MAX_SIZE];
  [[maybe_unused]] alignas(
      char32_t) char unsigned __buffer[sizeof(__ascii) * sizeof(char32_t)];
  switch (query) {
  case ::std::error_query_information::name: {
    *__scatters = __tls_name_message_range(encoding, 1u, 3u);
    __scatterlen = 1u;
    break;
  }
  case ::std::error_query_information::message:
    [[fallthrough]];
  case ::std::error_query_information::name_message: {
    if (::std::error_query_information::name_message == query) {
      *__scatters = __tls_name_message_range(encoding, 0u, 5u);
      ++__scatterlen;
    }
    /* "(<level>:<desc>)" in every mode; the literal+message tail is
       the part no-message builds strip */
    char unsigned *__ap{
        __format_decimal_pair_value_full_with_bracket<false, char unsigned>(
            __ascii, level, desc)};
    if constexpr (!__messages_disabled) {
      auto __level_scatter{__tls_level_literal(level)};
      auto __msg_scatter{__to_u8scatter_from_tls_alert(desc)};
      __builtin_memcpy(__ap, __level_scatter.base, __level_scatter.len);
      __ap += __level_scatter.len;
      __builtin_memcpy(__ap, __msg_scatter.base, __msg_scatter.len);
      __ap += __msg_scatter.len;
    }
    char unsigned const *__from_first{__ascii};
    char unsigned const *__from_last{__ap};
    {
      switch (encoding) {
#ifdef __LIBHERBCEPTIONS_ENABLE_EBCDIC
      case ::std::error_reporter_encoding::utfebcdic: {
        auto __dest{__write_ebcdic_with_ascii_only_range(
            __from_first, __from_last, __buffer)};
        __scatters[__scatterlen] = {
            __buffer,
            static_cast<::std::size_t>(
                reinterpret_cast<char unsigned *>(__dest) - __buffer)};
        break;
      }
#endif
      case ::std::error_reporter_encoding::utf16: {
        using __char16_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
            [[__gnu__::__may_alias__]]
#endif
            = char16_t *;
        auto __dest{__write_with_ascii_only_range(
            __from_first, __from_last,
            reinterpret_cast<__char16_may_alias_ptr>(__buffer))};
        __scatters[__scatterlen] = {
            __buffer,
            static_cast<::std::size_t>(
                reinterpret_cast<char unsigned *>(__dest) - __buffer)};
        break;
      }
      case ::std::error_reporter_encoding::utf32: {
        using __char32_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
            [[__gnu__::__may_alias__]]
#endif
            = char32_t *;
        auto __dest{__write_with_ascii_only_range(
            __from_first, __from_last,
            reinterpret_cast<__char32_may_alias_ptr>(__buffer))};
        __scatters[__scatterlen] = {
            __buffer,
            static_cast<::std::size_t>(
                reinterpret_cast<char unsigned *>(__dest) - __buffer)};
        break;
      }
      default: {
        __scatters[__scatterlen] = {__ascii,
                                    static_cast<::std::size_t>(__ap - __ascii)};
        break;
      }
      }
      ++__scatterlen;
    }
    break;
  }
  }
  cookfun(cookie, __scatters, __scatterlen);
}

constexpr ::std::error_domain_singleton tls_error_domain{
    .do_equivalent =
        [](::std::size_t cd, ::std::error_domain_singleton const *otherdomain,
           ::std::size_t othercd) noexcept {
          if (otherdomain == __builtin_addressof(tls_error_domain))
            return cd == othercd;
          return tls_error_domain.do_to_errc(cd) ==
                 otherdomain->do_to_errc(othercd);
        },
    .do_query_information = __tls_query_information,
    .do_to_errc = [](::std::size_t cd) noexcept -> ::std::errc {
      switch (static_cast<::std::tls_alert::alert_description>(
          static_cast<::std::uint_least8_t>(cd))) {
      case ::std::tls_alert::alert_description::close_notify:
      case ::std::tls_alert::alert_description::user_canceled:
        /* closures, not errors */
        return ::std::errc{};
      case ::std::tls_alert::alert_description::bad_certificate:
      case ::std::tls_alert::alert_description::unsupported_certificate:
      case ::std::tls_alert::alert_description::certificate_revoked:
      case ::std::tls_alert::alert_description::certificate_expired:
      case ::std::tls_alert::alert_description::certificate_unknown:
      case ::std::tls_alert::alert_description::bad_certificate_status_response:
      case ::std::tls_alert::alert_description::certificate_required:
      case ::std::tls_alert::alert_description::access_denied:
      case ::std::tls_alert::alert_description::unknown_ca:
        return ::std::errc::permission_denied;
      case ::std::tls_alert::alert_description::record_overflow:
#ifdef EMSGSIZE
        return ::std::errc::message_size;
#else
        return ::std::errc::value_too_large;
#endif
#ifdef EPROTONOSUPPORT
      case ::std::tls_alert::alert_description::protocol_version:
      case ::std::tls_alert::alert_description::insufficient_security:
      case ::std::tls_alert::alert_description::unsupported_extension:
      case ::std::tls_alert::alert_description::missing_extension:
      case ::std::tls_alert::alert_description::no_application_protocol:
      case ::std::tls_alert::alert_description::unrecognized_name:
      case ::std::tls_alert::alert_description::inappropriate_fallback:
        return ::std::errc::protocol_not_supported;
#endif
      case ::std::tls_alert::alert_description::internal_error:
      case ::std::tls_alert::alert_description::general_error:
        return ::std::errc::io_error;
      default:
    /* handshake_failure, decode_error, decrypt_error, bad_record_mac,
       unexpected_message, illegal_parameter, unknown_psk_identity and
       every unassigned code are peer-protocol failures */
#ifdef EPROTO
        return ::std::errc::protocol_error;
#else
        return ::std::errc::io_error;
#endif
      }
    }};
} // namespace

extern "C" __HERBCEPTIONS_API ::std::error_domain_singleton const *
__cxa_error_domain_tls() noexcept {
  return __builtin_addressof(tls_error_domain);
}
