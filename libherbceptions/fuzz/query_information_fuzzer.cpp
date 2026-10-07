//===--- query_information_fuzzer.cpp - query-information buffer fuzzer --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Feeds fuzz-controlled emit programs to
// ::std::error_domains::__cxa_error_query_information_create through a
// synthetic domain: the input picks the number of emit calls, the scatter count
// per call and every fragment's bytes and length (including zero), so the
// append sink is exercised across its growth, empty, NUL-pad and pass-through
// paths. The returned scatter is verified byte-for-byte against a shadow copy
// of everything emitted, then cloned and freed in fuzz-chosen order. Real
// domains are also driven through the same create/clone/free cycle.
//
//===----------------------------------------------------------------------===//

#include "fuzz_harness.h"

#include <cstdlib>
#include <cstring>

namespace {

constexpr ::std::size_t kMaxFrag{4096};
constexpr ::std::size_t kMaxTotal{1u << 18u};
constexpr ::std::size_t kMaxScatters{8};

// Per-input program state. The input bytes double as the fragment source:
// control bytes pick scatter counts and lengths, the remaining bytes are
// the fragment payloads. `shadow` is only read back up to `emitted`, so it
// does not need clearing between inputs.
struct hook_state {
  ::std::uint8_t const *cur{};
  ::std::uint8_t const *end{};
  ::std::size_t emitted{};
  ::std::size_t code_seen{};
  ::std::error_query_information info_seen{};
  ::std::error_reporter_encoding enc_seen{};
  char unsigned shadow[kMaxTotal];
};

hook_state g_state;

[[noreturn]] void fail() noexcept { ::std::abort(); }

::std::uint8_t next_byte() noexcept {
  return g_state.cur == g_state.end ? ::std::uint8_t{} : *g_state.cur++;
}

::std::size_t next_size(::std::size_t bound) noexcept {
  return ((static_cast<::std::size_t>(next_byte()) << 8u) | next_byte()) %
         bound;
}

void fuzz_query_information(
    ::std::size_t code, ::std::error_query_information info,
    ::std::error_reporter_encoding enc, void *cookie,
    ::std::error_reporter_io_cookie_function emit) noexcept {
  // The runtime must forward these unmodified.
  g_state.code_seen = code;
  g_state.info_seen = info;
  g_state.enc_seen = enc;
  ::std::io_scatter_t scatters[kMaxScatters];
  while (g_state.cur != g_state.end) {
    ::std::size_t const n{next_size(kMaxScatters + 1u)};
    for (::std::size_t i{}; i != n; ++i) {
      ::std::size_t want{next_size(kMaxFrag)};
      // Consume the flag byte before measuring the remaining input —
      // avail_in must be computed after every control byte for this
      // scatter, otherwise cur can advance past end.
      bool const degenerate{(next_byte() & 31u) == 0u};
      if (degenerate) {
        // Occasionally a degenerate scatter: zero len, sometimes null base.
        scatters[i] = {(next_byte() & 1u) != 0u ? nullptr : g_state.cur,
                       ::std::size_t{}};
        continue;
      }
      ::std::size_t const avail_in{
          static_cast<::std::size_t>(g_state.end - g_state.cur)};
      ::std::size_t const avail_cap{kMaxTotal - g_state.emitted};
      if (want > avail_in) {
        want = avail_in;
      }
      if (want > avail_cap) {
        want = avail_cap;
      }
      scatters[i] = {g_state.cur, want};
      __builtin_memcpy(g_state.shadow + g_state.emitted, g_state.cur, want);
      g_state.emitted += want;
      g_state.cur += want;
    }
    emit(cookie, scatters, n);
  }
}

// Aggregate-initialized: only do_query_information is set.
::std::error_domain_singleton const g_domain{
    nullptr, nullptr, fuzz_query_information, nullptr, nullptr};
::std::error_domain_singleton const g_hookless_domain{};

void check_empty(::std::io_scatter_t const &s) noexcept {
  if (s.len != 0 || s.base == nullptr) {
    fail();
  }
  // The empty result must still read as a NUL-terminated string in the
  // widest supported encoding (utf32): the first char32_t is zero.
  if (*static_cast<char32_t const *>(s.base) != 0) {
    fail();
  }
}

void check_full(::std::io_scatter_t const &s, char unsigned const *expect,
                ::std::size_t expect_len) noexcept {
  if (s.len != expect_len || s.base == nullptr) {
    fail();
  }
  if (expect_len == 0) {
    check_empty(s);
    return;
  }
  if (__builtin_memcmp(s.base, expect, expect_len) != 0) {
    fail();
  }
  // Every nonempty buffer carries a 4-byte NUL pad — inside the
  // allocation, so this read is also an ASan bounds check.
  auto const *p{static_cast<char unsigned const *>(s.base)};
  for (::std::size_t i{}; i != sizeof(char32_t); ++i) {
    if (p[expect_len + i] != 0) {
      fail();
    }
  }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const *data,
                                      ::std::size_t size) {
  if (size < 8) {
    return 0;
  }
  g_state.cur = data;
  g_state.end = data + size - 1;
  g_state.emitted = 0;
  g_state.code_seen = 0;
  ::std::uint8_t const ops{data[size - 1]};

  // Header bytes: the code/encoding/info arguments, deliberately allowing
  // out-of-range enum values — the runtime forwards them verbatim.
  ::std::size_t code{};
  for (::std::size_t i{}; i != sizeof(::std::size_t); ++i) {
    code = (code << 8u) | next_byte();
  }
  auto const enc{static_cast<::std::error_reporter_encoding>(next_byte())};
  auto const info{static_cast<::std::error_query_information>(next_byte())};

  // Synthetic domain: the emit program runs over whatever input is left.
  auto const res{::std::error_domains::__cxa_error_query_information_create(
      &g_domain, code, enc, info)};
  if (g_state.code_seen != code || g_state.enc_seen != enc ||
      g_state.info_seen != info) {
    fail();
  }
  // g_state.emitted/shadow are complete only now that the hook has run.
  check_full(res, g_state.shadow, g_state.emitted);

  // Full clone must be byte-identical and independently owned.
  auto const dup{::std::error_domains::__cxa_error_query_information_clone(
      res.base, res.len)};
  check_full(dup, g_state.shadow, res.len);

  // Cloning a shorter prefix is legal: the caller owns len bytes.
  auto const prelen{(ops >> 3u) % (res.len + 1u)};
  auto const pre{::std::error_domains::__cxa_error_query_information_clone(
      res.base, prelen)};
  check_full(pre, g_state.shadow, prelen);

  // clone() on non-buffer inputs has fixed contracts.
  auto const null_clone{
      ::std::error_domains::__cxa_error_query_information_clone(nullptr,
                                                                ops | 1u)};
  if (null_clone.base != nullptr || null_clone.len != 0) {
    fail();
  }
  check_empty(
      ::std::error_domains::__cxa_error_query_information_clone(res.base, 0));

  // free() ignores len==0 outright — even for non-heap bases.
  {
    char canary[8]{};
    ::std::error_domains::__cxa_error_query_information_free(canary, 0);
    for (char const c : canary) {
      if (c != 0) {
        fail();
      }
    }
  }

  if ((ops & 1u) != 0u) {
    ::std::error_domains::__cxa_error_query_information_free(dup.base, dup.len);
    ::std::error_domains::__cxa_error_query_information_free(res.base, res.len);
  } else {
    ::std::error_domains::__cxa_error_query_information_free(res.base, res.len);
    ::std::error_domains::__cxa_error_query_information_free(dup.base, dup.len);
  }
  ::std::error_domains::__cxa_error_query_information_free(pre.base, pre.len);

  // Degenerate domains: no hook, and no domain at all.
  check_empty(::std::error_domains::__cxa_error_query_information_create(
      &g_hookless_domain, code, enc, info));
  check_empty(::std::error_domains::__cxa_error_query_information_create(
      nullptr, code, enc, info));

  // A real domain through the same create/clone/free cycle.
  auto const real{::std::error_domains::__cxa_error_query_information_create(
      ::std::error_domains::__cxa_error_domain_posix(), code, enc, info)};
  auto const real_dup{::std::error_domains::__cxa_error_query_information_clone(
      real.base, real.len)};
  check_full(real_dup, static_cast<char unsigned const *>(real.base), real.len);
  ::std::error_domains::__cxa_error_query_information_free(real_dup.base,
                                                           real_dup.len);
  ::std::error_domains::__cxa_error_query_information_free(real.base, real.len);

  return 0;
}
