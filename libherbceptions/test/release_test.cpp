//===--- release_test.cpp - std::error::release() tests --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Tests std::error::release(): it moves the {domain, code} payload out as a
// cxx_std_error, rebinds the source error to {posix domain, 0}, and the
// released value can re-enter the channel via throw throws.
//
// RUN: %cxx %herbceptions_flags -I%herbceptions_include -L%herbceptions_lib -lherbceptions %s -o %t && %t | %FileCheck %s
// CHECK: PASS
//
//===----------------------------------------------------------------------===//

#include "herbceptions/error"

#include <cstdio>
#include <cstring>

static int failures = 0;
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

static void thrower() throws {
  throw throws ::std::errc::permission_denied;
}

static void rethrower(::std::cxx_std_error e) throws {
  CHECK(e.domain != nullptr); // never rethrow a null-domain value
  throw throws e;
}

int main() {
  auto const *posix = ::std::error_domains::__cxa_error_domain_posix();

  ::std::cxx_std_error rel{};
  CHECK(rel.domain == nullptr); // zero-init means "no error"

  // First hop: catch the error, inspect, release.
  try {
    thrower();
    CHECK(false && "unreached");
  } catch throws(::std::error e) {
    CHECK(e.domain() == posix);
    CHECK(e.code() == 13);

    rel = e.release();
    // Released payload carries the original domain + code.
    CHECK(rel.domain == posix);
    CHECK(rel.code == 13);
    // The rebound error is {posix, 0} — the success value; its destructor
    // has nothing to clean up.
    CHECK(e.domain() == posix);
    CHECK(e.code() == 0);
    CHECK(e == ::std::errc{});
  }

  // Second hop: put the released pair back on the channel.
  try {
    rethrower(rel);
    CHECK(false && "unreached");
  } catch throws(::std::error e) {
    CHECK(e.domain() == posix);
    CHECK(e.code() == 13);
    CHECK(e == ::std::errc::permission_denied);
  }
  // rel was consumed by the rethrow; the caller's copy is unchanged bits —
  // ownership-wise it is spent, nothing destroys it.

  // release() inside a constexpr-friendly path — see the Sema test for the
  // constant-evaluated version; here just verify the type is the builtin.
  static_assert(__is_same(::std::cxx_std_error, __CXX_STD_ERROR_TYPE__));
  static_assert(sizeof(::std::cxx_std_error) == 2 * sizeof(void *));

  if (failures == 0)
    std::printf("PASS\n");
  return failures;
}
