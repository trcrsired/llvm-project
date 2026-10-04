//===--- http_errc_test.cpp - std::http_errc domain tests ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Tests the http_errc error domain: domain identity, code() == wire status,
// do_to_errc mapping + class fallback, do_query_information text, unlisted
// codes, and a throws/catch round trip.
//
// RUN: %cxx %herbceptions_flags -I%herbceptions_include -L%herbceptions_lib -lherbceptions %s -o %t && %t | %FileCheck %s
// CHECK: PASS
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
  throw throws ::std::http_errc::not_found;
}

int main() {
  auto const *http = ::std::error_domains::__cxa_error_domain_http();
  CHECK(http != nullptr);
  CHECK(::std::error_domain<::std::http_errc>::domain() == http);

  // code() is the wire status number.
  CHECK(::std::error_domain<::std::http_errc>::code(
            ::std::http_errc::not_found) == 404);
  CHECK(::std::error_domain<::std::http_errc>::code(
            ::std::http_errc::continue_) == 100);

  // do_to_errc: strong mappings.
  CHECK(http->do_to_errc(404) == ::std::errc::no_such_file_or_directory);
  CHECK(http->do_to_errc(401) == ::std::errc::permission_denied);
  CHECK(http->do_to_errc(408) == ::std::errc::timed_out);
  CHECK(http->do_to_errc(413) == ::std::errc::file_too_large);
  CHECK(http->do_to_errc(501) == ::std::errc::function_not_supported);
  CHECK(http->do_to_errc(503) == ::std::errc::resource_unavailable_try_again);
  CHECK(http->do_to_errc(507) == ::std::errc::no_space_on_device);

  // do_to_errc: class fallbacks.
  CHECK(http->do_to_errc(200) == ::std::errc{}); // 2xx is not an error
  CHECK(http->do_to_errc(301) == ::std::errc{}); // 3xx either
  CHECK(http->do_to_errc(430) == ::std::errc::protocol_error); // unlisted 4xx
  CHECK(http->do_to_errc(599) == ::std::errc::io_error);       // unlisted 5xx
  CHECK(http->do_to_errc(700) == ::std::errc::io_error);       // out of registry

  // do_query_information.
  capture_ctx nctx{};
  http->do_query_information(404, ::std::error_query_information::name,
                             ::std::error_reporter_encoding::utf8, &nctx,
                             capture_cookfun);
  CHECK(std::string(nctx.buf, nctx.len) == "http");

  capture_ctx mctx{};
  http->do_query_information(404, ::std::error_query_information::message,
                             ::std::error_reporter_encoding::utf8, &mctx,
                             capture_cookfun);
  CHECK(std::string(mctx.buf, mctx.len) == "(404)Not Found");

  capture_ctx nmctx{};
  http->do_query_information(503,
                             ::std::error_query_information::name_message,
                             ::std::error_reporter_encoding::utf8, &nmctx,
                             capture_cookfun);
  CHECK(std::string(nmctx.buf, nmctx.len) ==
        "[http](503)Service Unavailable");

  // Unlisted code reports Unknown.
  capture_ctx uctx{};
  http->do_query_information(599, ::std::error_query_information::message,
                             ::std::error_reporter_encoding::utf8, &uctx,
                             capture_cookfun);
  CHECK(std::string(uctx.buf, uctx.len) == "(599)Unknown");

  // Same-domain equivalence by code; cross-domain via to_errc.
  CHECK(http->do_equivalent(404, http, 404));
  CHECK(!http->do_equivalent(404, http, 410));

  // throws/catch round trip.
  try {
    thrower();
    CHECK(false && "unreached");
  } catch throws(::std::error e) {
    CHECK(e.domain() == http);
    CHECK(e.code() == 404);
    CHECK(e == ::std::http_errc::not_found);
    CHECK(e.to_errc() == ::std::errc::no_such_file_or_directory);
  }

  // Unlisted codes ride the channel too.
  try {
    try {
      throw throws static_cast<::std::http_errc>(599);
    } catch throws(::std::error e) {
      CHECK(e.code() == 599);
      CHECK(e.to_errc() == ::std::errc::io_error);
      throw throws e; // rethrow
    }
    CHECK(false && "unreached");
  } catch throws(::std::error e) {
    CHECK(e.domain() == http);
    CHECK(e.code() == 599);
  }

  if (failures == 0)
    std::printf("PASS\n");
  return failures;
}
