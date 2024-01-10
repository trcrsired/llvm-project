//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Test that _LIBCPP_FREESTANDING is not defined when -ffreestanding is not passed
// to the compiler but defined when -ffreestanding is passed to the compiler.

// RUN: %{cxx} %{flags} %{compile_flags} -fsyntax-only %s
// RUN: %{cxx} %{flags} %{compile_flags} -fsyntax-only -ffreestanding -DFREESTANDING %s
// RUN: %{cxx} %{flags} %{compile_flags} -fsyntax-only -ffreestanding %s

#include <__config>

#if defined(FREESTANDING) != defined(_LIBCPP_FREESTANDING)
#  error _LIBCPP_FREESTANDING should be defined in freestanding mode and not \
       defined in non-freestanding mode
#  if defined(__has_feature)
#    if __has_feature(modules)
#      define _LIBCPP_FREESTANDING_NO_TEST_MODULES
#    endif
#  elif defined(__cpp_modules)
#    define _LIBCPP_FREESTANDING_NO_TEST_MODULES
#  endif

#  if defined(_LIBCPP_FREESTANDING) && !defined(_LIBCPP_FREESTANDING_NO_TEST_MODULES)
#    include <cstddef>
#    include <limits>
#    include <cfloat>
#    include <version>
#    include <cstdint>
#    include <cstdlib>
#    include <new>
#    include <typeinfo>
#    if __has_include(<source_location>)
#      include <source_location>
#    endif
#    include <exception>
#    include <initializer_list>
#    include <compare>
#    include <coroutine>
#    include <cstdarg>
#    include <concepts>
#    include <type_traits>
#    include <bit>
#    include <atomic>
#    include <utility>
#    include <tuple>
#    include <memory>
#    include <functional>
#    include <ratio>
#    include <iterator>
#    include <ranges>
#    include <typeinfo>

/*
We tested these headers are for preventing build issues
*/
#    include <iosfwd>
#    include <cstdio>
#    include <cerrno>
#    include <cstring>
#    include <cmath>
#    ifndef _LIBCPP_HAS_NO_WIDE_CHARACTERS
#      include <cwchar>
#    endif
#    include <array>
#    include <span>
#    include <algorithm>
#    include <bitset>
#    include <variant>
#    include <optional>
#    include <numbers>
#    include <scoped_allocator>
#    include <typeindex>
#    include <string_view>
#    include <numeric>
#    include <complex>
#    include <chrono>
#    include <charconv>
#    include <expected>
#    include <random>
#    include <any>

/*
+ * libc++ Containers Extensions
+ */
#    include <vector>
#    include <deque>
#    include <string>
#    include <stack>
#    include <queue>
#    include <list>
#    include <forward_list>
#    include <map>
#    include <set>
#    include <unordered_set>
#    include <unordered_map>
#    if __has_include(<mdspan>)
#      include <mdspan>
#    endif
#    include <valarray>

#    if _LIBCPP_STD_VER < 20
#      include <ciso646>
#      if __has_include(<cstdalign>)
#        include <cstdalign>
#      endif
#      include <cstdbool>
#    endif
#  endif
