//===--- coroutine_error_test.cpp - coroutine_error type test -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Tests std::coroutine_error, the coroutine-frame carrier for herbception
// errors. It has the same {domain, code} layout as std::error but is movable
// and nullable, so the promise's unhandled_herbception can store it in the
// frame and the awaiter can move it out and rethrow it.
//
// The type is only fabricated by the compiler (like std::error), so the
// runtime test exercises its public move/ownership surface; the type
// properties are checked with static_asserts.
//
//===----------------------------------------------------------------------===//

#include "herbceptions/error"

#include <type_traits>

// std::coroutine_error is brought in by herbceptions/error.
static_assert(!std::is_default_constructible_v<std::coroutine_error>,
              "coroutine_error is only fabricated by the compiler");
static_assert(!std::is_copy_constructible_v<std::coroutine_error>);
static_assert(!std::is_copy_assignable_v<std::coroutine_error>);
static_assert(std::is_move_constructible_v<std::coroutine_error>);
static_assert(std::is_move_assignable_v<std::coroutine_error>);
static_assert(std::is_nothrow_move_constructible_v<std::coroutine_error>);
static_assert(std::is_nothrow_move_assignable_v<std::coroutine_error>);
static_assert(std::is_trivially_destructible_v<std::coroutine_error> == false,
              "coroutine_error runs do_cleanup in its destructor");

// Same {domain, code} two-word layout as std::error.
static_assert(sizeof(std::coroutine_error) == sizeof(void *) + sizeof(std::size_t));

int main() { return 0; }
