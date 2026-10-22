//===--- query_information.cpp - query-information buffer ABI -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements __cxa_error_query_information_{create,clone,free}: the buffer
// plumbing behind std::cxx_std_error_legacy_exception's cached what() text.
// create() drives the domain's do_query_information hook once with an
// appendable cookie — the emit callback appends each scatter fragment into
// a growable buffer, like appending to a std::string — so the allocation
// stays inside the runtime and clone/free remain correct across module/CRT
// boundaries.
//
// Buffer contract: every returned scatter is NUL-terminated. Non-empty
// results are heap buffers carrying a 4-byte NUL pad (enough for the widest
// supported encoding, utf32); an empty result is the shared zero global —
// all-zero bytes terminate a string in every supported encoding. Callers
// must pass a len!=0 buffer to free()/clone() before it owns heap memory,
// which is why free() ignores len==0.
//
//===----------------------------------------------------------------------===//

// The buffer ABI is hosted-only: a freestanding build has no heap-backed
// query text to cache. MSVC /kernel defines _KERNEL_MODE but leaves
// __STDC_HOSTED__ at 1.
#if __STDC_HOSTED__ == 1 && !defined(_KERNEL_MODE)

#include "__malloc_or_heap_alloc_temp_buffer.h"
#include "libherbceptions.h"
#include <cstdlib>
#include <cstring>

namespace {

// The reporter's appendable sink: the domain's do_query_information calls
// the emit callback as many times as it likes, each call appending its
// scatter fragments onto this growable buffer — the cookie is, in effect,
// a std::string. Capacity always reserves room for the 4-byte NUL pad.
struct query_information_append_ctx {
  char unsigned *__base{};
  ::std::size_t __len{};
  ::std::size_t __cap{};
};

void query_information_append_cookie(void *__cookie,
                                     ::std::io_scatter_t const *__scatters,
                                     ::std::size_t __n) noexcept {
  auto &__ctx{*static_cast<query_information_append_ctx *>(__cookie)};
  for (::std::size_t __i{}; __i != __n; ++__i) {
    auto const __fraglen{__scatters[__i].len};
    if (__fraglen == 0) {
      continue;
    }
    ::std::size_t __need;
    if (__builtin_add_overflow(__ctx.__len, __fraglen,
                               __builtin_addressof(__need)) ||
        __builtin_add_overflow(__need, sizeof(char32_t),
                               __builtin_addressof(__need))) {
      ::std::abort();
    }
    if (__ctx.__cap < __need) {
      ::std::size_t __newcap{__ctx.__cap == 0 ? 256 : __ctx.__cap};
      while (__newcap < __need) {
        if (__builtin_mul_overflow(__newcap, 2,
                                   __builtin_addressof(__newcap))) {
          __newcap = __need;
          break;
        }
      }
      auto *__nb{static_cast<char unsigned *>(
          ::std::error_domains::__herbceptions_detail::
              __malloc_or_heap_alloc_or_die(__newcap))};
      if (__ctx.__len != 0) {
        __builtin_memcpy(__nb, __ctx.__base, __ctx.__len);
        ::std::error_domains::__herbceptions_detail::__free_or_heap_dealloc(
            __ctx.__base);
      }
      __ctx.__base = __nb;
      __ctx.__cap = __newcap;
    }
    __builtin_memcpy(__ctx.__base + __ctx.__len, __scatters[__i].base,
                     __fraglen);
    __ctx.__len += __fraglen;
  }
}

} // namespace

extern "C" {

__HERBCEPTIONS_API_ALLOC ::std::io_scatter_t
__cxa_error_query_information_create(
    ::std::error_domain_singleton const *__domain, ::std::size_t __code,
    ::std::error_reporter_encoding __encoding,
    ::std::error_query_information __info) noexcept {
  if (__domain == nullptr || __domain->do_query_information == nullptr) {
    return {U"", 0};
  }
  query_information_append_ctx __ctx{};
  __domain->do_query_information(__code, __info, __encoding,
                                 __builtin_addressof(__ctx),
                                 query_information_append_cookie);
  if (__ctx.__len == 0) {
    // Nothing reported: callers only own len!=0 buffers, so the
    // allocation (if the reporter appended then a domain bug left it
    // empty — unreachable) has to be released here.
    ::std::error_domains::__herbceptions_detail::__free_or_heap_dealloc(
        __ctx.__base);
    return {U"", 0};
  }
  __builtin_memset(__ctx.__base + __ctx.__len, 0, sizeof(char32_t));
  return {__ctx.__base, __ctx.__len};
}

__HERBCEPTIONS_API_ALLOC ::std::io_scatter_t
__cxa_error_query_information_clone(void const *__base,
                                    ::std::size_t __len) noexcept {
  if (__len == 0) {
    return {U"", 0};
  }
  if (__base == nullptr) {
    return {};
  }
  ::std::size_t __allocsize;
  if (__builtin_add_overflow(__len, sizeof(char32_t),
                             __builtin_addressof(__allocsize))) {
    ::std::abort();
  }
  auto *__buffer{static_cast<char unsigned *>(
      ::std::error_domains::__herbceptions_detail::
          __malloc_or_heap_alloc_or_die(__allocsize))};
  __builtin_memcpy(__buffer, __base, __len);
  __builtin_memset(__buffer + __len, 0, sizeof(char32_t));
  return {__buffer, __len};
}

__HERBCEPTIONS_API_ALLOC void
__cxa_error_query_information_free(void const *__base,
                                   ::std::size_t __len) noexcept {
  if (__len == 0) {
    // Empty results are the shared zero global, not heap storage.
    return;
  }
  ::std::error_domains::__herbceptions_detail::__free_or_heap_dealloc(
      const_cast<void *>(__base));
}

} // extern "C"

#endif
