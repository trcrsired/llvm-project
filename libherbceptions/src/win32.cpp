//===--- win32_domain.cpp - win32 (win32_errc) error domain ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the win32 (win32_errc, Win32 GetLastError codes) error_domain
// singleton vtable and the weak __cxa_error_domain_win32 ABI entry point.
// Only built on _WIN32/__CYGWIN__ targets.
//
//===----------------------------------------------------------------------===//

#if defined(_WIN32) || defined(__CYGWIN__)

#if 0
#include "win32_common.h"
#else
#include "domain_helpers.h"
#include <windows.h>
#undef min
#undef max
#include "__malloc_or_heap_alloc_temp_buffer.h"
#endif
namespace {

inline constexpr ::std::io_scatter_t
win32_name_message_range(::std::error_reporter_encoding encoding,
                         ::std::size_t startpos, ::std::size_t n) noexcept {
  switch (encoding) {
  case ::std::error_reporter_encoding::utfebcdic: {
    return {&startpos["\xAD\x97\x96\xA2\x89\xA7\xBD"], n};
  }
  case ::std::error_reporter_encoding::utf16: {
    return {&startpos[u"[win32]"], n * sizeof(char16_t)};
  }
  case ::std::error_reporter_encoding::utf32: {
    return {&startpos[U"[win32]"], n * sizeof(char32_t)};
  }
  default: {
    return {&startpos[u8"[win32]"], n};
  }
  }
}

inline constexpr ::std::io_scatter_t
win32_name(::std::error_reporter_encoding encoding) noexcept {
  return win32_name_message_range(encoding, 1u, 5u);
}

inline constexpr ::std::io_scatter_t
win32_name_message(::std::error_reporter_encoding encoding) noexcept {
  return win32_name_message_range(encoding, 0u, 7u);
}

inline constexpr bool use_9xa_apis{
#ifdef _WIN32_WINDOWS
    true
#endif
};

constinit ::std::error_domain_singleton win32_error_domain{
    .do_equivalent =
        [](::std::size_t cd, ::std::error_domain_singleton const *otherdomain,
           ::std::size_t othercd) noexcept {
          if (otherdomain == __builtin_addressof(win32_error_domain))
            return cd == othercd;
          return win32_error_domain.do_to_errc(cd) ==
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
          ::std::uint_least32_t win32err{
              static_cast<::std::uint_least32_t>(cd)};
          ::std::error_domains::__herbceptions_detail::__local_free_temp_buffer
              frombuffer;
          ::std::io_scatter_t scatters[4];
          ::std::io_scatter_t *pos{scatters};
          switch (query) {
          case ::std::error_query_information::name: {
            *pos = win32_name(encoding);
            ++pos;
            break;
          }
          case ::std::error_query_information::name_message:
            *pos = win32_name_message(encoding);
            ++pos;
            [[fallthrough]];
          case ::std::error_query_information::message: {
            using fromptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
                [[__gnu__::__may_alias__]]
#endif
                = ::std::conditional_t<use_9xa_apis, char unsigned *,
                                       char16_t *>;
            constexpr ::std::uint_least32_t flags{
                FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                FORMAT_MESSAGE_ALLOCATE_BUFFER};
            ::std::uint_least32_t dwlen{};
            if constexpr (use_9xa_apis) {
              dwlen =
                  FormatMessageA(flags, nullptr, win32err,
                                 MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                                 reinterpret_cast<char *>(__builtin_addressof(
                                     frombuffer.__bufferptr)),
                                 0, nullptr);
            } else {
              using wcharmayaliasptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
                  [[__gnu__::__may_alias__]]
#endif
                  = wchar_t *;
              dwlen = FormatMessageW(
                  flags, nullptr, win32err,
                  MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                  reinterpret_cast<wcharmayaliasptr>(
                      __builtin_addressof(frombuffer.__bufferptr)),
                  0, nullptr);
            }
            ::std::error_domains::__herbceptions_detail::
                __malloc_or_heapalloc_temp_buffer destbuffer;
            if (dwlen) {
              auto frombufferptr{
                  reinterpret_cast<fromptr>(frombuffer.__bufferptr)};
              if (1 < dwlen && frombufferptr[dwlen - 2] == u8'\r' &&
                  frombufferptr[dwlen - 1] == u8'\n') {
                dwlen -= 2; // strip out \r\n
              }
              fromptr __from_first{frombufferptr},
                  __from_last{__from_first + dwlen};
              switch (encoding) {
              case ::std::error_reporter_encoding::utfebcdic: {
                auto buffer{
                    reinterpret_cast<char unsigned *>(frombuffer.__bufferptr)};
                auto dest{::std::error_domains::__herbceptions_detail::
                              __write_ebcdic_with_ascii_only_range(
                                  __from_first, __from_last, buffer)};
                ;
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
                if constexpr (sizeof(::std::size_t) <= sizeof(dwlen)) {
                  constexpr ::std::size_t mxval{static_cast<::std::size_t>(-1) /
                                                sizeof(char32_t)};
                  if (mxval < dwlen) {
                    ::std::abort();
                  }
                }
                auto buffer{reinterpret_cast<char unsigned *>(
                    ::std::error_domains::__herbceptions_detail::
                        __malloc_or_heap_alloc_or_die(
                            static_cast<::std::size_t>(dwlen) *
                            sizeof(char32_t)))};
                destbuffer.__bufferptr = buffer;
                auto dest = ::std::error_domains::__herbceptions_detail::
                    __write_with_ascii_only_range(
                        __from_first, __from_last,
                        reinterpret_cast<__char32_may_alias_ptr>(buffer));
                *pos = {buffer,
                        static_cast<::std::size_t>(
                            reinterpret_cast<char unsigned *>(dest) - buffer)};
                break;
              }
              case ::std::error_reporter_encoding::utf8:
              case ::std::error_reporter_encoding::gb18030: {
                if constexpr (use_9xa_apis) {
                  *pos = {__from_first,
                          static_cast<::std::size_t>(
                              reinterpret_cast<char unsigned *>(__from_last) -
                              reinterpret_cast<char unsigned *>(__from_first))};
                } else {
                  auto buffer{reinterpret_cast<char unsigned *>(
                      frombuffer.__bufferptr)};
                  auto dest{::std::error_domains::__herbceptions_detail::
                                __write_with_ascii_only_range(
                                    __from_first, __from_last, buffer)};
                  *pos = {buffer, static_cast<::std::size_t>(
                                      reinterpret_cast<char unsigned *>(dest) -
                                      buffer)};
                }
                break;
              }
              case ::std::error_reporter_encoding::utf16: {
                if constexpr (use_9xa_apis) {
                  using __char16_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
                      [[__gnu__::__may_alias__]]
#endif
                      = char16_t *;
                  if constexpr (sizeof(::std::size_t) <= sizeof(dwlen)) {
                    constexpr ::std::size_t mxval{
                        static_cast<::std::size_t>(-1) / sizeof(char16_t)};
                    if (mxval < dwlen) {
                      ::std::abort();
                    }
                  }
                  auto buffer{reinterpret_cast<char unsigned *>(
                      ::std::error_domains::__herbceptions_detail::
                          __malloc_or_heap_alloc_or_die(
                              static_cast<::std::size_t>(dwlen) *
                              sizeof(char16_t)))};
                  destbuffer.__bufferptr = buffer;
                  auto __dest = ::std::error_domains::__herbceptions_detail::
                      __write_with_ascii_only_range(
                          __from_first, __from_last,
                          reinterpret_cast<__char16_may_alias_ptr>(buffer));
                  *pos = {
                      buffer,
                      static_cast<::std::size_t>(
                          reinterpret_cast<char unsigned *>(__dest) - buffer)};
                  break;
                }
                [[fallthrough]];
              }
              default: {
                *pos = {__from_first,
                        static_cast<::std::size_t>(
                            reinterpret_cast<char unsigned *>(__from_last) -
                            reinterpret_cast<char unsigned *>(__from_first))};
                break;
              }
              }
              ++pos;
            }
            break;
          }
          }
          cookfun(cookie, scatters, static_cast<::std::size_t>(pos - scatters));
        },
    .do_to_errc = [](::std::size_t cd) noexcept -> ::std::errc {
      switch (static_cast<::std::uint_least32_t>(cd)) {
#include "win32_errc_map.hpp"
      }
      return ::std::errc::io_error;
    }};
} // namespace

extern "C" __HERBCEPTIONS_API ::std::error_domain_singleton const *
__cxa_error_domain_win32() noexcept {
  return __builtin_addressof(win32_error_domain);
}

#endif // _WIN32 || __CYGWIN__
