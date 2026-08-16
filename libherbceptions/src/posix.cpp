//===--- posix.cpp - posix (std::errc) error domain -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the posix (std::errc) error_domain_singleton vtable and the weak
// __cxa_error_domain_posix ABI entry point. Available on all platforms.
// The errno message table lives in src/domain_helpers.h.
//
//===----------------------------------------------------------------------===//

#include "herbceptions/__details/posix.h"
#include "domain_helpers.h"

namespace std::__error_domains {

namespace __herbceptions_detail {

constexpr ::std::io_scatter_t __to_u8scatter_from_errno(int __eno) noexcept {
  using std::__error_domains::__herbceptions_detail::__tsc;
  switch (__eno) {
#include "posix_table.hpp"
  }
}
} // namespace __herbceptions_detail

constinit ::std::error_domain_singleton __posix_error_domain{
    .do_cleanup = nullptr, // errno values need no cleanup
    .do_equivalent =
        [](::std::size_t cd, ::std::error_domain_singleton const *otherdomain,
           ::std::size_t othercd) noexcept {
          return __posix_error_domain.do_to_errc(cd) ==
                 otherdomain->do_to_errc(othercd);
        },
    .do_query_information =
        [](::std::size_t cd, ::std::error_query_information query,
           ::std::error_reporter_encoding encoding, void *cookie,
           ::std::error_reporter_io_cookie_function cookfun) noexcept {
          if (static_cast<::std::uint_least32_t>(
                  ::std::error_query_information::name_message) <
              static_cast<::std::uint_least32_t>(query)) {
            return;
          }
          ::std::io_scatter_t __scatters[2];
          auto __pos{__scatters};
          constexpr ::std::size_t __errno_max_bytes{POSIX_ERRNO_MAX_SIZE *
                                                    sizeof(char32_t)};
          alignas(char32_t) char unsigned __buffer[__errno_max_bytes];
          switch (query) {
          case ::std::error_query_information::name: {
            switch (encoding) {
            case ::std::error_reporter_encoding::utfebcdic: {
              *__pos = {"\x97\x96\xA2\x89\xA7", 5u};
              break;
            }
            case ::std::error_reporter_encoding::utf16: {
              *__pos = {u"posix", 5u * sizeof(char16_t)};
              break;
            }
            case ::std::error_reporter_encoding::utf32: {
              *__pos = {U"posix", 5u * sizeof(char16_t)};
              break;
            }
            default: {
              *__pos = {u8"posix", 5u * sizeof(char8_t)};
              break;
            }
            }
            ++__pos;
            break;
          }
          case ::std::error_query_information::name_message: {
            switch (encoding) {
            case ::std::error_reporter_encoding::utfebcdic: {
              *__pos = {"\xAD\x97\x96\xA2\x89\xA7\xBD", 7u};
              break;
            }
            case ::std::error_reporter_encoding::utf16: {
              *__pos = {u"[posix]", 7u * sizeof(char16_t)};
              break;
            }
            case ::std::error_reporter_encoding::utf32: {
              *__pos = {U"[posix]", 7u * sizeof(char16_t)};
              break;
            }
            default: {
              *__pos = {u8"[posix]", 7u * sizeof(char8_t)};
              break;
            }
            }
            ++__pos;
            [[fallthrough]];
          }
          default: {
            auto __scatter{::std::__error_domains::__herbceptions_detail::
                               __to_u8scatter_from_errno(static_cast<int>(
                                   static_cast<unsigned>(cd)))};
            char unsigned const *__from_first{
                reinterpret_cast<char unsigned const *>(__scatter.base)};
            char unsigned const *__from_last{__from_first + __scatter.len};
            switch (encoding) {
            case ::std::error_reporter_encoding::utfebcdic: {
              auto __dest = ::std::__error_domains::__herbceptions_detail::
                  __write_ebcdic_with_ascii_only_range(__from_first,
                                                       __from_last, __buffer);
              *__pos = {
                  __buffer,
                  static_cast<::std::size_t>(
                      reinterpret_cast<char unsigned *>(__dest) - __buffer)};
            }
            case ::std::error_reporter_encoding::utf16: {
              using __char16_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
                  [[__gnu__::__may_alias__]]
#endif
                  = char16_t *;
              auto __dest = ::std::__error_domains::__herbceptions_detail::
                  __write_with_ascii_only_range(
                      __from_first, __from_last,
                      reinterpret_cast<__char16_may_alias_ptr>(__buffer));
              *__pos = {
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
              auto __dest = ::std::__error_domains::__herbceptions_detail::
                  __write_with_ascii_only_range(
                      __from_first, __from_last,
                      reinterpret_cast<__char32_may_alias_ptr>(__buffer));
              *__pos = {
                  __buffer,
                  static_cast<::std::size_t>(
                      reinterpret_cast<char unsigned *>(__dest) - __buffer)};
              break;
            }
            default: {
              *__pos = __scatter;
              break;
            }
            }
            ++__pos;
          }
          }
          cookfun(cookie, __scatters,
                  static_cast<::std::size_t>(__pos - __scatters));
        },
    .do_to_errc =
        [](::std::size_t cd) noexcept { return static_cast<::std::errc>(cd); }};

extern "C" [[__gnu__::__weak__]]
::std::error_domain_singleton const *__cxa_error_domain_posix() noexcept {
  return __builtin_addressof(::std::__error_domains::__posix_error_domain);
}

} // namespace std::__error_domains
