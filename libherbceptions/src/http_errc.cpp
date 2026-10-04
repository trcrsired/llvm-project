//===--- http_errc.cpp - http_errc error domain ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the http_errc error_domain_singleton vtable and the
// __cxa_error_domain_http ABI entry point. Available on all platforms.
// Deliberately self-contained — it does NOT include
// simple_query_information_common.h, so linking the domain does not drag
// the shared table machinery (and every other domain's strings) into the
// binary. Name/message strings come from src/http_table.hpp.
//
//===----------------------------------------------------------------------===//

#include "libherbceptions.h"
#include <herbceptions/__details/http.h>

namespace {

using ::std::error_domains::__herbceptions_detail::
    __format_decimal_value_full_with_bracket;
using ::std::error_domains::__herbceptions_detail::
    __format_decimal_value_max_size_with_brackets;
using ::std::error_domains::__herbceptions_detail::__messages_disabled;
using ::std::error_domains::__herbceptions_detail::__tsc;
using ::std::error_domains::__herbceptions_detail::
    __write_with_ascii_only_range;
#ifdef __LIBHERBCEPTIONS_ENABLE_EBCDIC
using ::std::error_domains::__herbceptions_detail::
    __write_ebcdic_with_ascii_only_range;
#endif

inline constexpr ::std::io_scatter_t
__http_name_message_range(::std::error_reporter_encoding encoding,
                          ::std::size_t startpos, ::std::size_t n) noexcept {
  switch (encoding) {
#ifdef __LIBHERBCEPTIONS_ENABLE_EBCDIC
  case ::std::error_reporter_encoding::utfebcdic: {
    return {&startpos["\xAD\x88\xA3\xA3\x97\xBD"], n};
  }
#endif
  case ::std::error_reporter_encoding::utf16: {
    return {&startpos[u"[http]"], n * sizeof(char16_t)};
  }
  case ::std::error_reporter_encoding::utf32: {
    return {&startpos[U"[http]"], n * sizeof(char32_t)};
  }
  default: {
    return {&startpos[u8"[http]"], n};
  }
  }
}

inline constexpr ::std::io_scatter_t
__to_u8scatter_from_http_errc(::std::uint_least32_t __eno) noexcept {
  switch (__eno) {
#include "http_table.hpp"
  }
}

/*
Writes "(<decimal code>)" for the requested encoding into __numbuf and
returns it as a scatter. Local copy of the shared helper so this TU does
not pull in simple_query_information_common.h.
*/
inline constexpr ::std::io_scatter_t
__http_code_scatter(::std::error_reporter_encoding encoding,
                    ::std::uint_least32_t __code,
                    char unsigned *__numbuf) noexcept {
  switch (encoding) {
#ifdef __LIBHERBCEPTIONS_ENABLE_EBCDIC
  case ::std::error_reporter_encoding::utfebcdic: {
    auto *__dest{__format_decimal_value_full_with_bracket<true, char unsigned>(
        __numbuf, __code)};
    return {__numbuf, static_cast<::std::size_t>(__dest - __numbuf)};
  }
#endif
  case ::std::error_reporter_encoding::utf16: {
    using __char16_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
        [[__gnu__::__may_alias__]]
#endif
        = char16_t *;
    auto *__dest{__format_decimal_value_full_with_bracket<false, char16_t>(
        reinterpret_cast<__char16_may_alias_ptr>(__numbuf), __code)};
    return {__numbuf,
            static_cast<::std::size_t>(
                reinterpret_cast<char unsigned *>(__dest) - __numbuf)};
  }
  case ::std::error_reporter_encoding::utf32: {
    using __char32_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
        [[__gnu__::__may_alias__]]
#endif
        = char32_t *;
    auto *__dest{__format_decimal_value_full_with_bracket<false, char32_t>(
        reinterpret_cast<__char32_may_alias_ptr>(__numbuf), __code)};
    return {__numbuf,
            static_cast<::std::size_t>(
                reinterpret_cast<char unsigned *>(__dest) - __numbuf)};
  }
  default: {
    auto *__dest{__format_decimal_value_full_with_bracket<false, char unsigned>(
        __numbuf, __code)};
    return {__numbuf, static_cast<::std::size_t>(__dest - __numbuf)};
  }
  }
}

inline void __http_query_information(
    ::std::size_t cd, ::std::error_query_information query,
    ::std::error_reporter_encoding encoding, void *cookie,
    ::std::error_reporter_io_cookie_function cookfun) noexcept {
  if (static_cast<::std::uint_least32_t>(
          ::std::error_query_information::name_message) <
      static_cast<::std::uint_least32_t>(query)) {
    return;
  }
  auto const __code{static_cast<::std::uint_least32_t>(cd)};
  ::std::io_scatter_t __scatters[3];
  ::std::size_t __scatterlen{};
  alignas(char32_t) char unsigned __numbuf
      [__format_decimal_value_max_size_with_brackets<::std::uint_least32_t> *
       sizeof(char32_t)];
  switch (query) {
  case ::std::error_query_information::name: {
    *__scatters = __http_name_message_range(encoding, 1u, 4u);
    __scatterlen = 1u;
    break;
  }
  case ::std::error_query_information::message:
    [[fallthrough]];
  case ::std::error_query_information::name_message: {
    if (::std::error_query_information::name_message == query) {
      *__scatters = __http_name_message_range(encoding, 0u, 6u);
      ++__scatterlen;
    }
    __scatters[__scatterlen] = __http_code_scatter(encoding, __code, __numbuf);
    ++__scatterlen;
    if constexpr (!__messages_disabled) {
      /*
      Message text lives in a fixed table; transcode it into the requested
      encoding through __buffer. With __messages_disabled only
      "[http](code)" is ever reported.
      */
      constexpr ::std::size_t __max_message_bytes{HTTP_ERRC_MAX_SIZE *
                                                  sizeof(char32_t)};
      alignas(char32_t) char unsigned __buffer[__max_message_bytes];
      auto __scatter{__to_u8scatter_from_http_errc(__code)};
      char unsigned const *__from_first{
          reinterpret_cast<char unsigned const *>(__scatter.base)};
      char unsigned const *__from_last{__from_first + __scatter.len};
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
        __scatters[__scatterlen] = __scatter;
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

constinit ::std::error_domain_singleton http_error_domain{
    .do_equivalent =
        [](::std::size_t cd, ::std::error_domain_singleton const *otherdomain,
           ::std::size_t othercd) noexcept {
          if (otherdomain == __builtin_addressof(http_error_domain))
            return cd == othercd;
          return http_error_domain.do_to_errc(cd) ==
                 otherdomain->do_to_errc(othercd);
        },
    .do_query_information = __http_query_information,
    .do_to_errc = [](::std::size_t cd) noexcept -> ::std::errc {
      switch (static_cast<::std::http_errc>(cd)) {
      case ::std::http_errc::bad_request:
      case ::std::http_errc::length_required:
      case ::std::http_errc::precondition_failed:
      case ::std::http_errc::unprocessable_content:
        return ::std::errc::invalid_argument;
      case ::std::http_errc::unauthorized:
      case ::std::http_errc::forbidden:
      case ::std::http_errc::proxy_authentication_required:
      case ::std::http_errc::unavailable_for_legal_reasons:
      case ::std::http_errc::network_authentication_required:
        return ::std::errc::permission_denied;
      case ::std::http_errc::not_found:
      case ::std::http_errc::gone:
        return ::std::errc::no_such_file_or_directory;
      case ::std::http_errc::method_not_allowed:
        return ::std::errc::operation_not_supported;
      case ::std::http_errc::request_timeout:
      case ::std::http_errc::gateway_timeout:
        return ::std::errc::timed_out;
      case ::std::http_errc::conflict:
        return ::std::errc::file_exists;
      case ::std::http_errc::content_too_large:
        return ::std::errc::file_too_large;
      case ::std::http_errc::uri_too_long:
        return ::std::errc::filename_too_long;
      case ::std::http_errc::range_not_satisfiable:
        return ::std::errc::invalid_seek;
      case ::std::http_errc::locked:
      case ::std::http_errc::too_early:
      case ::std::http_errc::too_many_requests:
      case ::std::http_errc::service_unavailable:
        return ::std::errc::resource_unavailable_try_again;
      case ::std::http_errc::request_header_fields_too_large:
        return ::std::errc::argument_list_too_long;
      case ::std::http_errc::not_implemented:
        return ::std::errc::function_not_supported;
      case ::std::http_errc::http_version_not_supported:
      case ::std::http_errc::not_extended:
        return ::std::errc::protocol_not_supported;
      case ::std::http_errc::insufficient_storage:
        return ::std::errc::no_space_on_device;
      case ::std::http_errc::loop_detected:
        return ::std::errc::too_many_symbolic_link_levels;
      default: {
        // Unlisted codes fall back by class: 4xx are protocol problems,
        // 5xx+ are server I/O failures, and anything below 400 is not an
        // error at all.
        if (cd < 400) {
          return ::std::errc{};
        }
        if (cd < 500) {
          return ::std::errc::protocol_error;
        }
        return ::std::errc::io_error;
      }
      }
    }};
} // namespace

extern "C" __HERBCEPTIONS_API ::std::error_domain_singleton const *
__cxa_error_domain_http() noexcept {
  return __builtin_addressof(http_error_domain);
}
