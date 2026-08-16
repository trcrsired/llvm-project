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

#include "herbceptions/__details/win32.h"
#include "domain_helpers.h"
#include "ntkernel.h"

#if defined(_WIN32) || defined(__CYGWIN__)

namespace std::error_domains {
namespace {
using namespace __herbceptions_detail;

::std::errc win32_to_errc(::std::uint_least32_t cd) noexcept {
  switch (static_cast<::std::win32_errc>(cd)) {
  case ::std::win32_errc::success:
    return static_cast<::std::errc>(0);
  case ::std::win32_errc::access_denied:
    return ::std::errc::permission_denied;
  case ::std::win32_errc::file_not_found:
  case ::std::win32_errc::path_not_found:
    return ::std::errc::no_such_file_or_directory;
  case ::std::win32_errc::invalid_function:
  case ::std::win32_errc::invalid_parameter:
    return ::std::errc::invalid_argument;
  case ::std::win32_errc::not_enough_memory:
  case ::std::win32_errc::out_of_memory:
  case ::std::win32_errc::insufficient_buffer:
    return ::std::errc::not_enough_memory;
  case ::std::win32_errc::file_exists:
  case ::std::win32_errc::already_exists:
    return ::std::errc::file_exists;
  case ::std::win32_errc::broken_pipe:
    return ::std::errc::broken_pipe;
  case ::std::win32_errc::too_many_open_files:
    return ::std::errc::too_many_files_open;
  case ::std::win32_errc::disk_full:
  case ::std::win32_errc::handle_disk_full:
    return ::std::errc::no_space_on_device;
  case ::std::win32_errc::operation_aborted:
  case ::std::win32_errc::canceled:
    return ::std::errc::operation_canceled;
  case ::std::win32_errc::sharing_violation:
  case ::std::win32_errc::lock_violation:
    return ::std::errc::device_or_resource_busy;
  case ::std::win32_errc::not_supported:
    return ::std::errc::not_supported;
  default:
    return ::std::errc::io_error;
  }
}

// win32 <-> nt equivalence via the table's win32 column (reverse lookup: a
// Win32 code matches an NTSTATUS row whose win32 column equals it).
bool win32_equivalent_nt(::std::size_t win32cd, ::std::size_t ntcd) noexcept {
  if (ntkernel_field const *f =
          find_ntstatus(static_cast<::std::uint_least32_t>(ntcd)))
    return static_cast<::std::size_t>(f->win32) == win32cd;
  return false;
}

// Find the first table row whose win32 column equals the given Win32 code.
// The table is sorted by NTSTATUS, not by win32, so this is a linear scan.
ntkernel_field const *find_win32_message(::std::uint_least32_t win32cd) noexcept {
  for (ntkernel_field const &f : ntkernel_table)
    if (f.win32 == win32cd)
      return __builtin_addressof(f);
  return nullptr;
}

constinit ::std::error_domain_singleton __win32_error_domain{
    .do_cleanup = nullptr,
    .do_equivalent =
        [](::std::size_t cd, ::std::error_domain_singleton const *otherdomain,
           ::std::size_t othercd) noexcept {
          // win32 <-> win32: identity.
          if (otherdomain == __builtin_addressof(__win32_error_domain))
            return cd == othercd;
          // win32 <-> nt: use the table's win32 column.
          if (otherdomain == ::std::error_domains::__cxa_error_domain_nt())
            return win32_equivalent_nt(cd, othercd);
          // win32 <-> any other domain: compare via the POSIX errno mapping.
          return win32_to_errc(static_cast<::std::uint_least32_t>(cd)) ==
                 otherdomain->do_to_errc(othercd);
        },
    .do_query_information =
        [](::std::size_t cd, ::std::error_query_information query,
           ::std::error_reporter_encoding encoding, void *cookie,
           ::std::error_reporter_io_cookie_function cookfun) noexcept {
          query_information_pieces pieces;
          switch (query) {
          case ::std::error_query_information::name:
            pieces.add_cstr(u8"win32");
            break;
          case ::std::error_query_information::message:
            // The win32 code 0 means success; the table row for it is
            // STATUS_SUCCESS, so look up the message through the win32 column.
            if (ntkernel_field const *f = find_win32_message(
                    static_cast<::std::uint_least32_t>(cd)))
              pieces.add(f->message, f->message_size);
            break;
          case ::std::error_query_information::name_message:
            pieces.add_cstr(u8"win32");
            if (ntkernel_field const *f = find_win32_message(
                    static_cast<::std::uint_least32_t>(cd)))
              pieces.add(f->message, f->message_size);
            break;
          }
          pieces.emit(encoding, cookie, cookfun);
        },
    .do_to_errc =
        [](::std::size_t cd) noexcept {
          return win32_to_errc(static_cast<::std::uint_least32_t>(cd));
        }};
} // namespace

extern "C" [[__gnu__::__weak__]]
::std::error_domain_singleton const *__cxa_error_domain_win32() noexcept {
  return __builtin_addressof(::std::error_domains::__win32_error_domain);
}

} // namespace std::error_domains

#endif // _WIN32 || __CYGWIN__
