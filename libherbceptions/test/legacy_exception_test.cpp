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
// do_throw_legacy_exception vtable entry, so the call falls back to
// std::system_error(to_errc(), generic_category()). Also exercises a plain
// C++ exception escaping a throws function -> herbception conversion.
//
// RUN: %cxx %herbceptions_flags -I%herbceptions_include -L%herbceptions_lib
// -lherbceptions %s -o %t && %t | %FileCheck %s CHECK: PASS
//
//===----------------------------------------------------------------------===//

#include "herbceptions/error"

#include <cstdio>
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

int main() {
  // Fallback path: posix has no do_throw_legacy_exception, so the error is
  // raised as std::system_error(to_errc(), generic_category()).
  try {
    thrower();
    CHECK(false && "unreached");
  } catch throws(::std::error e) {
    try {
      e.throw_legacy_exception();
      CHECK(false && "unreached");
    } catch (::std::system_error const &se) {
      CHECK(se.code().value() ==
            static_cast<int>(::std::errc::permission_denied));
      CHECK(se.code().category() == ::std::generic_category());
    } catch (...) {
      CHECK(false && "expected std::system_error");
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

  if (failures == 0)
    std::printf("PASS\n");
  return failures;
}
