#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)

#include "libherbceptions.h"
#include "win32_imports.h"
#include "__malloc_or_heap_alloc_temp_buffer.h"

namespace std::error_domains::__herbceptions_detail {

inline constexpr bool __win32_use_9xa_apis{
#ifdef _WIN32_WINDOWS
    true
#endif
};

/*
Fetches the FormatMessage system text for a Win32 error code and reports it,
converted to the requested encoding, through cookfun as a single scatter.
Reports nothing when no message is available. Internal temporary buffers are
released before return; the collector must consume the bytes immediately.
*/
inline void __report_win32_message_text(
    ::std::uint_least32_t win32err, ::std::error_reporter_encoding encoding,
    void *cookie, ::std::error_reporter_io_cookie_function cookfun) noexcept {
  using fromptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
      [[__gnu__::__may_alias__]]
#endif
      = ::std::conditional_t<__win32_use_9xa_apis, char unsigned *, char16_t *>;
  constexpr ::std::uint_least32_t flags{win32::format_message_from_system |
                                        win32::format_message_ignore_inserts |
                                        win32::format_message_allocate_buffer};
  __local_free_temp_buffer frombuffer;
  ::std::uint_least32_t dwlen{};
  if constexpr (__win32_use_9xa_apis) {
    dwlen = win32::FormatMessageA(
        flags, nullptr, win32err, HB_MAKE_LANGID(win32::lang_english, win32::sublang_english_us),
        reinterpret_cast<char *>(__builtin_addressof(frombuffer.__bufferptr)),
        0, nullptr);
  } else {
    using wcharmayaliasptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
        [[__gnu__::__may_alias__]]
#endif
        = wchar_t *;
    dwlen = win32::FormatMessageW(flags, nullptr, win32err,
                           HB_MAKE_LANGID(win32::lang_english, win32::sublang_english_us),
                           reinterpret_cast<wcharmayaliasptr>(
                               __builtin_addressof(frombuffer.__bufferptr)),
                           0, nullptr);
  }
  if (!dwlen) {
    return;
  }
  auto frombufferptr{reinterpret_cast<fromptr>(frombuffer.__bufferptr)};
  if (1 < dwlen && frombufferptr[dwlen - 2] == u8'\r' &&
      frombufferptr[dwlen - 1] == u8'\n') {
    dwlen -= 2; // strip out \r\n
  }
  fromptr __from_first{frombufferptr}, __from_last{__from_first + dwlen};
  __heapalloc_temp_buffer destbuffer;
  ::std::io_scatter_t scatter{};
  switch (encoding) {
  case ::std::error_reporter_encoding::utfebcdic: {
    auto buffer{reinterpret_cast<char unsigned *>(frombuffer.__bufferptr)};
    auto dest{__write_ebcdic_with_ascii_only_range(__from_first, __from_last,
                                                   buffer)};
    scatter = {buffer, static_cast<::std::size_t>(
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
    auto buffer{reinterpret_cast<char unsigned *>(__win32_heap_alloc_or_die(
        static_cast<::std::size_t>(dwlen) * sizeof(char32_t)))};
    destbuffer.__bufferptr = buffer;
    auto dest{__write_with_ascii_only_range(
        __from_first, __from_last,
        reinterpret_cast<__char32_may_alias_ptr>(buffer))};
    scatter = {buffer, static_cast<::std::size_t>(
                           reinterpret_cast<char unsigned *>(dest) - buffer)};
    break;
  }
  case ::std::error_reporter_encoding::utf8:
  case ::std::error_reporter_encoding::gb18030: {
    if constexpr (__win32_use_9xa_apis) {
      scatter = {__from_first,
                 static_cast<::std::size_t>(
                     reinterpret_cast<char unsigned *>(__from_last) -
                     reinterpret_cast<char unsigned *>(__from_first))};
    } else {
      auto buffer{reinterpret_cast<char unsigned *>(frombuffer.__bufferptr)};
      auto dest{
          __write_with_ascii_only_range(__from_first, __from_last, buffer)};
      scatter = {buffer, static_cast<::std::size_t>(
                             reinterpret_cast<char unsigned *>(dest) - buffer)};
    }
    break;
  }
  case ::std::error_reporter_encoding::utf16: {
    if constexpr (__win32_use_9xa_apis) {
      using __char16_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
          [[__gnu__::__may_alias__]]
#endif
          = char16_t *;
      if constexpr (sizeof(::std::size_t) <= sizeof(dwlen)) {
        constexpr ::std::size_t mxval{static_cast<::std::size_t>(-1) /
                                      sizeof(char16_t)};
        if (mxval < dwlen) {
          ::std::abort();
        }
      }
      auto buffer{
          reinterpret_cast<char unsigned *>(__win32_heap_alloc_or_die(
              static_cast<::std::size_t>(dwlen) * sizeof(char16_t)))};
      destbuffer.__bufferptr = buffer;
      auto __dest{__write_with_ascii_only_range(
          __from_first, __from_last,
          reinterpret_cast<__char16_may_alias_ptr>(buffer))};
      scatter = {buffer,
                 static_cast<::std::size_t>(
                     reinterpret_cast<char unsigned *>(__dest) - buffer)};
      break;
    }
    [[fallthrough]];
  }
  default: {
    scatter = {__from_first,
               static_cast<::std::size_t>(
                   reinterpret_cast<char unsigned *>(__from_last) -
                   reinterpret_cast<char unsigned *>(__from_first))};
    break;
  }
  }
  cookfun(cookie, __builtin_addressof(scatter), 1u);
}

} // namespace std::error_domains::__herbceptions_detail

#endif // _WIN32 || __CYGWIN__
