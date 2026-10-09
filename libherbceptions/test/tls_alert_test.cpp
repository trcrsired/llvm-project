//===--- tls_alert_test.cpp - std::tls_alert domain tests -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Tests the tls alert error domain: domain identity, code() == the packed
// (alert_level, alert_description) wire pair, do_to_errc mapping,
// do_query_information text, unlisted codes/levels, and a throws/catch
// round trip preserving both wire bytes.
//
// RUN: %cxx %herbceptions_flags -I%herbceptions_include -L%herbceptions_lib
// -lherbceptions %s -o %t && %t | %FileCheck %s CHECK: PASS
//
//===----------------------------------------------------------------------===//

#include "herbceptions/error"

#include <cstdio>
#include <cstring>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

namespace {
struct capture_ctx {
  char buf[512];
  std::size_t len;
};
} // namespace

static void capture_cookfun(void *cookie, ::std::io_scatter_t const *v,
                            ::std::size_t n) noexcept {
  auto *ctx = static_cast<capture_ctx *>(cookie);
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < v[i].len; ++j) {
      if (ctx->len < sizeof(ctx->buf) - 1)
        ctx->buf[ctx->len++] = static_cast<char const *>(v[i].base)[j];
    }
  }
  ctx->buf[ctx->len] = '\0';
}

static void thrower() throws {
  throw throws::std::tls_alert{::std::tls_alert::alert_level::fatal,
                               ::std::tls_alert::alert_description::unknown_ca};
}

int main() {
  using alert = ::std::tls_alert;
  using level = ::std::tls_alert::alert_level;
  using desc = ::std::tls_alert::alert_description;

  auto const *tls = ::std::error_domains::__cxa_error_domain_tls();
  CHECK(tls != nullptr);
  CHECK(::std::error_domain<alert>::domain() == tls);

  // code() packs the two wire bytes: (level << 8) | description.
  CHECK(::std::error_domain<alert>::code(
            alert{level::fatal, desc::unknown_ca}) == 560);
  CHECK(::std::error_domain<alert>::code(
            alert{level::warning, desc::close_notify}) == 256);
  CHECK(::std::error_domain<alert>::code(
            alert{level::fatal, desc::decode_error}) == 562);

  // do_to_errc: closures are not errors, cert/auth alerts are
  // permission_denied, negotiation alerts are protocol_not_supported,
  // and everything else is a peer-protocol failure.
  CHECK(tls->do_to_errc(256) == ::std::errc{}); // warning close_notify
  CHECK(tls->do_to_errc(0) == ::std::errc{});   // fatal close_notify
  CHECK(tls->do_to_errc((2u << 8) | 42u) == ::std::errc::permission_denied);
  CHECK(tls->do_to_errc((2u << 8) | 45u) == ::std::errc::permission_denied);
  CHECK(tls->do_to_errc((2u << 8) | 48u) == ::std::errc::permission_denied);
  CHECK(tls->do_to_errc((2u << 8) | 116u) == ::std::errc::permission_denied);
  CHECK(tls->do_to_errc((2u << 8) | 22u) == ::std::errc::message_size);
  CHECK(tls->do_to_errc((2u << 8) | 70u) ==
        ::std::errc::protocol_not_supported);
  CHECK(tls->do_to_errc((2u << 8) | 120u) ==
        ::std::errc::protocol_not_supported);
  CHECK(tls->do_to_errc((2u << 8) | 80u) == ::std::errc::io_error);
  CHECK(tls->do_to_errc((2u << 8) | 117u) == ::std::errc::io_error);
  CHECK(tls->do_to_errc((2u << 8) | 50u) == ::std::errc::protocol_error);
  CHECK(tls->do_to_errc((2u << 8) | 200u) ==
        ::std::errc::protocol_error); // unassigned description

  // do_query_information: "[domain](level:desc)Literal: Message".
  capture_ctx nctx{};
  tls->do_query_information(560, ::std::error_query_information::name,
                            ::std::error_reporter_encoding::utf8, &nctx,
                            capture_cookfun);
  CHECK(std::string(nctx.buf, nctx.len) == "tls");

  capture_ctx mctx{};
  tls->do_query_information(560, ::std::error_query_information::message,
                            ::std::error_reporter_encoding::utf8, &mctx,
                            capture_cookfun);
  CHECK(std::string(mctx.buf, mctx.len) == "(2:48)Fatal: Unknown CA");

  capture_ctx nmctx{};
  tls->do_query_information(560, ::std::error_query_information::name_message,
                            ::std::error_reporter_encoding::utf8, &nmctx,
                            capture_cookfun);
  CHECK(std::string(nmctx.buf, nmctx.len) == "[tls](2:48)Fatal: Unknown CA");

  // Warning level prints its literal.
  capture_ctx wctx{};
  tls->do_query_information(256, ::std::error_query_information::message,
                            ::std::error_reporter_encoding::utf8, &wctx,
                            capture_cookfun);
  CHECK(std::string(wctx.buf, wctx.len) == "(1:0)Warning: Close Notify");

  // Unlisted description + unknown level byte still render.
  capture_ctx uctx{};
  tls->do_query_information(
      (7u << 8) | 200u, ::std::error_query_information::name_message,
      ::std::error_reporter_encoding::utf8, &uctx, capture_cookfun);
  CHECK(std::string(uctx.buf, uctx.len) == "[tls](7:200)Unknown: Unknown");

  // Same-domain equivalence requires BOTH wire bytes to match: a warning
  // close_notify and a fatal one are different codes.
  CHECK(tls->do_equivalent(560, tls, 560));
  CHECK(!tls->do_equivalent(560, tls, 561));
  CHECK(!tls->do_equivalent(256, tls, 0));

  // throws/catch round trip preserves level and description.
  try {
    thrower();
    CHECK(false && "unreached");
  } catch throws(::std::error e) {
    CHECK(e.domain() == tls);
    CHECK(e.code() == 560);
    CHECK((e == alert{level::fatal, desc::unknown_ca}));
    CHECK((e != alert{level::warning, desc::unknown_ca}));
    CHECK(e.to_errc() == ::std::errc::permission_denied);
    auto const a{::std::herbception_cast<alert>(e)};
    CHECK(a.level == level::fatal);
    CHECK(a.description == desc::unknown_ca);
  }

  // A peer-sent warning-level alert keeps its level byte on the wire.
  try {
    try {
      throw throws(alert{level::warning, desc::user_canceled});
    } catch throws(::std::error e) {
      CHECK(e.code() == ((1u << 8) | 90u));
      CHECK(e.to_errc() == ::std::errc{});
      throw throws e; // rethrow
    }
    CHECK(false && "unreached");
  } catch throws(::std::error e) {
    CHECK(e.domain() == tls);
    CHECK(e.code() == 346);
  }

  if (failures == 0)
    std::printf("PASS\n");
  return failures;
}
