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

// Largest stack buffer used when widening a message to UTF-16/UTF-32.
constexpr ::std::size_t nt_message_capacity = 32;

// The ntkernel table is entirely ASCII, so widening to UTF-16/UTF-32 is a
// simple per-byte copy (each char is a code point < 0x80); no codecvt needed.
template <typename CodeUnit>
::std::size_t widen_ascii(char8_t const *msg, CodeUnit *buf,
                          ::std::size_t capacity) noexcept {
  ::std::size_t n = 0;
  while (msg[n] != 0 && n < capacity) {
    buf[n] = static_cast<CodeUnit>(msg[n]);
    ++n;
  }
  return n;
}

// Emit the ASCII message \p msg, converting to the requested encoding. For
// utf16/utf32 the widened text is built into a fixed stack buffer (max
// nt_message_capacity code units) before the cookie call; utf8/gb18030 are
// byte-oriented and emitted directly.
void emit_message(char8_t const *msg, ::std::error_reporter_encoding encoding,
                  void *cookie,
                  ::std::error_reporter_io_cookie_function cookfun) noexcept {
  switch (encoding) {
  case ::std::error_reporter_encoding::utf8:
  case ::std::error_reporter_encoding::gb18030:
  case ::std::error_reporter_encoding::utfebcdic:
    write_ascii(encoding, cookie, cookfun, msg);
    return;
  case ::std::error_reporter_encoding::utf16: {
    char16_t buf[nt_message_capacity];
    ::std::size_t const n = widen_ascii(msg, buf, nt_message_capacity);
    write_text(encoding, cookie, cookfun, buf, n * sizeof(char16_t));
    return;
  }
  case ::std::error_reporter_encoding::utf32: {
    char32_t buf[nt_message_capacity];
    ::std::size_t const n = widen_ascii(msg, buf, nt_message_capacity);
    write_text(encoding, cookie, cookfun, buf, n * sizeof(char32_t));
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
