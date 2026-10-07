//===--- legacy_exception_test.cpp - throw_legacy_exception tests
//----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Tests std::error::throw_legacy_exception(): a posix-domain error has no
// do_throw_legacy_exception vtable entry, so the call falls back to boxing
// the {domain, code} pair into std::cxx_std_error_legacy_exception — which
// round-trips back to the throws channel via rethrow_herbception_if_error.
// Also exercises a plain C++ exception escaping a throws function ->
// herbception conversion.
//
// RUN: %cxx %herbceptions_flags -I%herbceptions_include -L%herbceptions_lib
// -lherbceptions %s -o %t && %t | %FileCheck %s CHECK: PASS
//
//===----------------------------------------------------------------------===//

#include "herbceptions/error"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <system_error>

static int failures = 0;
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

static void thrower() throws { throw throws::std::errc::permission_denied; }

// A legacy C++ exception thrown inside a throws function is converted to a
// herbception error at the boundary.
static void legacy_thrower() throws {
  throw ::std::system_error(
      ::std::make_error_code(::std::errc::no_such_file_or_directory));
}

// Throws the legacy-EH box out of a throws function: the boundary reboxes
// it under the exception_ptr domain.
static void legacy_relay() throws {
  try {
    thrower();
    CHECK(false && "unreached");
  } catch throws(::std::error e) {
    e.throw_legacy_exception();
  }
}

// The doc's bridge pattern: a plain (non-throws) wrapper forwards a channel
// error through legacy EH; a throws function probes the result for our box
// and puts the payload back on the channel.
static void io_call() throws {
  throw throws::std::errc::device_or_resource_busy;
}

static void legacy_facing_wrapper() {
  try {
    io_call();
  } catch throws(::std::error e) {
    e.throw_legacy_exception_no_what();
  }
}

static void on_channel() throws {
  try {
    legacy_facing_wrapper();
  } catch throws(::std::error e) {
    if (auto *le{::std::exception_cast_std_error<
            ::std::cxx_std_error_legacy_exception *>(e)}) {
      le->rethrow_herbception_if_error();
    }
    throw throws;
  }
}

// A foreign legacy exception escapes into the same handler: not our box,
// so the channel error propagates unchanged.
static void foreign_facing() { throw ::std::runtime_error("boom"); }

static void on_channel_foreign() throws {
  try {
    foreign_facing();
  } catch throws(::std::error e) {
    if (auto *le{::std::exception_cast_std_error<
            ::std::cxx_std_error_legacy_exception *>(e)}) {
      le->rethrow_herbception_if_error();
    }
    throw throws;
  }
}

int main() {
  // Fallback path: posix has no do_throw_legacy_exception, so the error is
  // boxed into std::cxx_std_error_legacy_exception carrying the {domain,
  // code} pair verbatim (posix owns nothing — no demotion needed).
  try {
    thrower();
    CHECK(false && "unreached");
  } catch throws(::std::error e) {
    // Not an exception_ptr code — the probe returns nullptr and
    // make_exception_ptr yields an empty exception_ptr.
    CHECK(::std::exception_cast_std_error<
              ::std::cxx_std_error_legacy_exception const *>(e) == nullptr);
    CHECK(::std::make_exception_ptr_from_std_error(e) == nullptr);
    try {
      e.throw_legacy_exception();
      CHECK(false && "unreached");
    } catch (::std::cxx_std_error_legacy_exception le) {
      CHECK(le.domain() == ::std::error_domains::__cxa_error_domain_posix());
      CHECK(le.code() ==
            static_cast<::std::size_t>(::std::errc::permission_denied));
      // "[posix](13)Permission denied" — the cached name+message.
      CHECK(::std::strstr(le.what(), "(13)") != nullptr);
      CHECK(::std::strstr(le.what(), "ermission") != nullptr);
      // Legacy box -> herbception channel round-trip.
      try {
        le.rethrow_herbception_if_error();
        CHECK(false && "unreached");
      } catch throws(::std::error e2) {
        CHECK(e2 == ::std::errc::permission_denied);
      }
    } catch (...) {
      CHECK(false && "expected std::cxx_std_error_legacy_exception");
    }

    // Fast sibling: no message query — what() is empty, the {domain, code}
    // payload is identical and still round-trips.
    try {
      e.throw_legacy_exception_no_what();
      CHECK(false && "unreached");
    } catch (::std::cxx_std_error_legacy_exception const &le) {
      CHECK(le.domain() == ::std::error_domains::__cxa_error_domain_posix());
      CHECK(le.code() ==
            static_cast<::std::size_t>(::std::errc::permission_denied));
      CHECK(le.what_scatter().len == 0);
      CHECK(le.what()[0] == '\0');
    } catch (...) {
      CHECK(false && "expected std::cxx_std_error_legacy_exception");
    }
  }

  // Legacy -> herbception: a system_error escaping a throws function lands
  // as a channel error under the platform exception_ptr domain.
  try {
    legacy_thrower();
    CHECK(false && "unreached");
  } catch throws(::std::error e) {
    CHECK(e.is_code_of<::std::exception_ptr>());
    // system_error's errc meaning is recovered through to_errc.
    CHECK(e.to_errc() == ::std::errc::no_such_file_or_directory);
    // The generic probe reaches the boxed object without rethrowing:
    // exact type, public base, and a negative match.
    if (auto const *se{
            ::std::exception_cast_std_error<::std::system_error const *>(e)}) {
      CHECK(se->code().value() ==
            static_cast<int>(::std::errc::no_such_file_or_directory));
    } else {
      CHECK(false && "expected exception_cast_std_error<system_error>");
    }
    CHECK(::std::exception_cast_std_error<::std::exception const *>(e) !=
          nullptr);
    CHECK(::std::exception_cast_std_error<::std::invalid_argument const *>(e) ==
          nullptr);
    // Materialize a real exception_ptr for STL interop.
    {
      auto ep{::std::make_exception_ptr_from_std_error(e)};
      CHECK(ep != nullptr);
      try {
        ::std::rethrow_exception(ep);
        CHECK(false && "unreached");
      } catch (::std::system_error const &se) {
        CHECK(se.code().value() ==
              static_cast<int>(::std::errc::no_such_file_or_directory));
      }
    }
    // ... and rethrowing it as a legacy exception recovers the
    // system_error itself.
    try {
      e.throw_legacy_exception();
      CHECK(false && "unreached");
    } catch (::std::system_error const &se) {
      CHECK(se.code().value() ==
            static_cast<int>(::std::errc::no_such_file_or_directory));
    } catch (...) {
      CHECK(false && "expected std::system_error");
    }
  }

  // Double hop: a cxx_std_error_legacy_exception escaping a throws
  // function is reboxed under the exception_ptr domain, which detects our
  // box and forwards to_errc/equivalent into the carried {domain, code}.
  try {
    legacy_relay();
    CHECK(false && "unreached");
  } catch throws(::std::error e) {
    CHECK(e.is_code_of<::std::exception_ptr>());
    CHECK(e.to_errc() == ::std::errc::permission_denied);
    CHECK(e.equivalent(::std::errc::permission_denied));
    // The probe sees through the exception_ptr domain to the box itself.
    if (auto const *box{::std::exception_cast_std_error<
            ::std::cxx_std_error_legacy_exception const *>(e)}) {
      CHECK(box->domain() == ::std::error_domains::__cxa_error_domain_posix());
      CHECK(box->code() ==
            static_cast<::std::size_t>(::std::errc::permission_denied));
    } else {
      CHECK(false && "expected exception_cast_std_error<the box>");
    }
    CHECK(::std::exception_cast_std_error<::std::system_error const *>(e) ==
          nullptr);
    {
      auto ep{::std::make_exception_ptr_from_std_error(e)};
      CHECK(ep != nullptr);
      try {
        ::std::rethrow_exception(ep);
        CHECK(false && "unreached");
      } catch (::std::cxx_std_error_legacy_exception const &le) {
        CHECK(le.domain() == ::std::error_domains::__cxa_error_domain_posix());
        CHECK(le.code() ==
              static_cast<::std::size_t>(::std::errc::permission_denied));
      }
    }
    // Rethrowing legacy-side recovers the box itself, still carrying the
    // posix pair and its original message.
    try {
      e.throw_legacy_exception();
      CHECK(false && "unreached");
    } catch (::std::cxx_std_error_legacy_exception le) {
      CHECK(le.domain() == ::std::error_domains::__cxa_error_domain_posix());
      CHECK(le.code() ==
            static_cast<::std::size_t>(::std::errc::permission_denied));
      CHECK(::std::strstr(le.what(), "(13)") != nullptr);
    } catch (...) {
      CHECK(false && "expected std::cxx_std_error_legacy_exception");
    }
  }

  // The doc's bridge pattern: our box survives the
  // throws -> legacy-EH -> throws hop and its payload rethrown by
  // rethrow_herbception_if_error equals the original error.
  try {
    on_channel();
    CHECK(false && "unreached");
  } catch throws(::std::error e) {
    CHECK(e.equivalent(::std::errc::device_or_resource_busy));
  }

  // A foreign exception is not our box: the probe misses and the
  // exception_ptr-domain error propagates unchanged through throw throws.
  try {
    on_channel_foreign();
    CHECK(false && "unreached");
  } catch throws(::std::error e) {
    CHECK(e.is_code_of<::std::exception_ptr>());
    CHECK(::std::exception_cast_std_error<
              ::std::cxx_std_error_legacy_exception *>(e) == nullptr);
    CHECK(::std::exception_cast_std_error<::std::runtime_error const *>(e) !=
          nullptr);
  }

  if (failures == 0)
    std::printf("PASS\n");
  return failures;
}
