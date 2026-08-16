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

#include "herbceptions/__details/nt.h"
#include "domain_helpers.h"
#include "herbceptions/__details/win32.h"
#include "ntkernel.h"

#if defined(_WIN32) || defined(__CYGWIN__)

#include <cerrno>
#include <cstdint>

namespace std::error_domains {
namespace {
using namespace __herbceptions_detail;

// Largest stack buffer used when converting a UTF-8 message to UTF-16/UTF-32.
constexpr ::std::size_t nt_message_capacity = 32;

// Append the UTF-8 code point at *&p (advancing p) to an out buffer of 16- or
// 32-bit code units. Returns false on overlong/invalid sequences.
template <typename CodeUnit>
bool append_utf8_codepoint(char const *&p, char const *end, CodeUnit *&out,
                           CodeUnit *out_end) noexcept {
  unsigned char c = static_cast<unsigned char>(*p);
  ::std::uint32_t cp;
  unsigned len;
  if (c < 0x80) {
    cp = c;
    len = 1;
  } else if ((c >> 5) == 0x6) {
    cp = c & 0x1F;
    len = 2;
  } else if ((c >> 4) == 0xE) {
    cp = c & 0x0F;
    len = 3;
  } else if ((c >> 3) == 0x1E) {
    cp = c & 0x07;
    len = 4;
  } else {
    return false;
  }
  if (static_cast<::std::size_t>(end - p) < len)
    return false;
  for (unsigned i = 1; i < len; ++i) {
    unsigned char cc = static_cast<unsigned char>(p[i]);
    if ((cc >> 6) != 0x2)
      return false;
    cp = (cp << 6) | (cc & 0x3F);
  }
  p += len;

  if constexpr (sizeof(CodeUnit) == 2) {
    // UTF-16: surrogate pair for code points above U+FFFF.
    if (cp > 0xFFFF) {
      if (out + 2 > out_end)
        return false;
      cp -= 0x10000;
      *out++ = static_cast<CodeUnit>(0xD800 + (cp >> 10));
      *out++ = static_cast<CodeUnit>(0xDC00 + (cp & 0x3FF));
      return true;
    }
  }
  if (out >= out_end)
    return false;
  *out++ = static_cast<CodeUnit>(cp);
  return true;
}

// Emit the UTF-8 message \p msg, converting to the requested encoding. For
// utf16/utf32 the converted text is built into a fixed stack buffer (max
// nt_message_capacity code units) before the cookie call; utf8/gb18030 are
// byte-oriented and emitted directly.
void emit_message(char8_t const *msg, ::std::error_reporter_encoding encoding,
                  void *cookie,
                  ::std::error_reporter_io_cookie_function cookfun) noexcept {
  char const *bytes = reinterpret_cast<char const *>(msg);
  switch (encoding) {
  case ::std::error_reporter_encoding::utf8:
  case ::std::error_reporter_encoding::gb18030:
  case ::std::error_reporter_encoding::utfebcdic:
    write_ascii(encoding, cookie, cookfun, msg);
    return;
  case ::std::error_reporter_encoding::utf16: {
    char16_t buf[nt_message_capacity];
    char16_t *out = buf;
    char16_t *out_end = buf + nt_message_capacity;
    char const *p = bytes;
    char const *end = p + __builtin_strlen(bytes);
    while (p != end)
      if (!append_utf8_codepoint(p, end, out, out_end))
        break;
    write_text(encoding, cookie, cookfun, buf,
               static_cast<::std::size_t>(out - buf) * sizeof(char16_t));
    return;
  }
  case ::std::error_reporter_encoding::utf32: {
    char32_t buf[nt_message_capacity];
    char32_t *out = buf;
    char32_t *out_end = buf + nt_message_capacity;
    char const *p = bytes;
    char const *end = p + __builtin_strlen(bytes);
    while (p != end)
      if (!append_utf8_codepoint(p, end, out, out_end))
        break;
    write_text(encoding, cookie, cookfun, buf,
               static_cast<::std::size_t>(out - buf) * sizeof(char32_t));
    return;
  }
  }
}

::std::errc nt_to_errc(int cd) noexcept {
  if (ntkernel_field const *f = find_ntstatus(cd))
    return static_cast<::std::errc>(f->posix);
  switch (static_cast<unsigned>(cd) >> 30) {
  case 0:
    return static_cast<::std::errc>(0);
  case 3:
  default:
    return ::std::errc::io_error;
  }
}

// nt <-> win32 equivalence via the table's win32 column.
bool nt_equivalent_win32(int cd, ::std::size_t win32cd) noexcept {
  if (ntkernel_field const *f = find_ntstatus(cd))
    return static_cast<::std::size_t>(static_cast<unsigned>(f->win32)) ==
           win32cd;
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
            return nt_equivalent_win32(static_cast<int>(cd), othercd);
          // nt <-> any other domain: compare via the POSIX errno mapping.
          return nt_to_errc(static_cast<int>(cd)) ==
                 otherdomain->do_to_errc(othercd);
        },
    .do_name =
        [](::std::size_t, ::std::error_reporter_encoding encoding, void *cookie,
           ::std::error_reporter_io_cookie_function cookfun) noexcept {
          write_ascii(encoding, cookie, cookfun, u8"nt");
        },
    .do_message =
        [](::std::size_t cd, ::std::error_reporter_encoding encoding,
           void *cookie,
           ::std::error_reporter_io_cookie_function cookfun) noexcept {
          if (cd == 0) {
            emit_message(u8"The operation completed successfully", encoding,
                         cookie, cookfun);
            return;
          }
          if (ntkernel_field const *f = find_ntstatus(static_cast<int>(cd))) {
            emit_message(f->message, encoding, cookie, cookfun);
            return;
          }
          switch (static_cast<unsigned>(cd) >> 30) {
          case 0:
            emit_message(u8"Unknown success", encoding, cookie, cookfun);
            return;
          case 1:
            emit_message(u8"Unknown information", encoding, cookie, cookfun);
            return;
          case 2:
            emit_message(u8"Unknown warning", encoding, cookie, cookfun);
            return;
          case 3:
            emit_message(u8"Unknown error", encoding, cookie, cookfun);
            return;
          }
        },
    .do_to_errc =
        [](::std::size_t cd) noexcept {
          return nt_to_errc(static_cast<int>(cd));
        }};
} // namespace

extern "C" [[__gnu__::__weak__]]
::std::error_domain_singleton const *__cxa_error_domain_nt() noexcept {
  return __builtin_addressof(::std::error_domains::__nt_error_domain);
}

} // namespace std::error_domains

#endif // _WIN32 || __CYGWIN__
