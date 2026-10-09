//===--- tls_alert_fuzzer.cpp - tls alert domain code fuzzer --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Drives the tls alert domain's do_query_information (all queries x all
// encodings), do_to_errc and do_equivalent with attacker-controlled packed
// (level, description) codes. The description byte is the IANA TLS Alerts
// value; the level byte is attacker-controlled and only 1/2 are assigned,
// so every unlisted level/description combination gets exercised.
// Everything here is memory safe by construction; the point is to shake
// out formatting / conversion / table bugs.
//
//===----------------------------------------------------------------------===//

#include "fuzz_harness.h"

namespace {

using ::std::error_domain_singleton;
using ::std::error_query_information;
using ::std::error_reporter_encoding;

constexpr ::std::error_query_information kQueries[] = {
    error_query_information::name,
    error_query_information::message,
    error_query_information::name_message,
};

constexpr ::std::error_reporter_encoding kEncodings[] = {
    error_reporter_encoding::utf8,      error_reporter_encoding::utf16,
    error_reporter_encoding::utf32,     error_reporter_encoding::gb18030,
    error_reporter_encoding::utfebcdic,
};

void query_all(error_domain_singleton const *domain,
               ::std::uint_least32_t code) noexcept {
  herbceptions_fuzz::capture_ctx ctx;
  for (auto const q : kQueries) {
    for (auto const e : kEncodings) {
      herbceptions_fuzz::reset(ctx);
      domain->do_query_information(code, q, e, &ctx,
                                   herbceptions_fuzz::capture_cookfun);
    }
  }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const *data,
                                      ::std::size_t size) {
  if (size < 8) {
    return 0;
  }
  auto const a{static_cast<::std::uint_least32_t>(
      data[0] | (data[1] << 8u) | (data[2] << 16u) | (data[3] << 24u))};
  auto const b{static_cast<::std::uint_least32_t>(
      data[4] | (data[5] << 8u) | (data[6] << 16u) | (data[7] << 24u))};

  auto const *tls{::std::error_domains::__cxa_error_domain_tls()};

  // Whole 32-bit space: out-of-range level bits above byte 0 and values
  // beyond the wire bytes must format without overflowing the buffers.
  query_all(tls, a);
  query_all(tls, b);

  // Codes worth hammering: both assigned levels against every description
  // byte, then fuzz level/description bytes through the full 16-bit pair
  // range.
  for (::std::uint_least32_t d{}; d != 256u; ++d) {
    query_all(tls, (1u << 8) | d);
    query_all(tls, (2u << 8) | d);
  }
  query_all(tls, ((a & 0xffu) << 8) | (b & 0xffu));
  query_all(tls, (a & 0xffffu));
  query_all(tls, a | (b << 16));

  // Equivalence: same domain cross-code, and tls vs posix through
  // do_to_errc on both sides.
  tls->do_equivalent(a, tls, b);
  auto const *posix{::std::error_domains::__cxa_error_domain_posix()};
  tls->do_equivalent(a, posix, b);
  posix->do_equivalent(a, tls, b);
  tls->do_to_errc(a);
  tls->do_to_errc(b);

  return 0;
}
