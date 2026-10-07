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
// create() drives the domain's do_query_information hook twice — once to
// size the reported text and once to copy it into a fresh buffer — so the
// allocation stays inside the runtime and clone/free remain correct across
// module/CRT boundaries.
//
// Buffer contract: every returned scatter is NUL-terminated. Non-empty
// results are heap buffers carrying a 4-byte NUL pad (enough for the widest
// supported encoding, utf32); an empty result is the shared zero global —
// all-zero bytes terminate a string in every supported encoding. Callers
// must pass a len!=0 buffer to free()/clone() before it owns heap memory,
// which is why free() ignores len==0.
//
//===----------------------------------------------------------------------===//

#if defined(__cpp_exceptions)
#include "__malloc_or_heap_alloc_temp_buffer.h"
#include "libherbceptions.h"
#include <cstdlib>
#include <cstring>

namespace {

// The shared empty message: one char32_t NUL is four zero bytes, i.e. a
// terminator for utf8, utf16, utf32, gb18030 and EBCDIC alike.
constinit char32_t __query_information_empty[1]{};

struct query_information_copy_ctx {
  char unsigned *__cur;
  char unsigned *__end;
};

void query_information_size_cookie(void *__cookie,
                                   ::std::io_scatter_t const *__scatters,
                                   ::std::size_t __n) noexcept {
  auto &__total{*static_cast<::std::size_t *>(__cookie)};
  for (::std::size_t __i{}; __i != __n; ++__i) {
    __total += __scatters[__i].len;
  }
}

void query_information_copy_cookie(void *__cookie,
                                   ::std::io_scatter_t const *__scatters,
                                   ::std::size_t __n) noexcept {
  auto &__ctx{*static_cast<query_information_copy_ctx *>(__cookie)};
  for (::std::size_t __i{}; __i != __n; ++__i) {
    auto __len{__scatters[__i].len};
    auto const __avail{static_cast<::std::size_t>(__ctx.__end - __ctx.__cur)};
    if (__avail < __len) {
      // A domain that reports more bytes on the second pass than on the
      // first would scribble past the buffer; clamp instead.
      __len = __avail;
    }
    __builtin_memcpy(__ctx.__cur, __scatters[__i].base, __len);
    __ctx.__cur += __len;
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
    return {__query_information_empty, 0};
  }
  ::std::size_t __total{};
  __domain->do_query_information(__code, __info, __encoding,
                                 __builtin_addressof(__total),
                                 query_information_size_cookie);
  if (__total == 0) {
    return {__query_information_empty, 0};
  }
  ::std::size_t __allocsize;
  if (__builtin_add_overflow(__total, sizeof(char32_t),
                             __builtin_addressof(__allocsize))) {
    ::std::abort();
  }
  auto *__buffer{static_cast<char unsigned *>(
      ::std::error_domains::__herbceptions_detail::
          __malloc_or_heap_alloc_or_die(__allocsize))};
  query_information_copy_ctx __ctx{__buffer, __buffer + __total};
  __domain->do_query_information(__code, __info, __encoding,
                                 __builtin_addressof(__ctx),
                                 query_information_copy_cookie);
  if (__ctx.__cur == __buffer) {
    // Second pass came back empty: callers only own len!=0 buffers, so the
    // allocation has to be released here before handing back the global.
    ::std::error_domains::__herbceptions_detail::__free_or_heap_dealloc(
        __buffer);
    return {__query_information_empty, 0};
  }
  __builtin_memset(__ctx.__cur, 0, sizeof(char32_t));
  return {__buffer, static_cast<::std::size_t>(__ctx.__cur - __buffer)};
}

__HERBCEPTIONS_API_ALLOC ::std::io_scatter_t
__cxa_error_query_information_clone(void const *__base,
                                    ::std::size_t __len) noexcept {
  if (__len == 0) {
    return {__query_information_empty, 0};
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
