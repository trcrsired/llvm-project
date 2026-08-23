#pragma once
/*
Shared plumbing for the libherbceptions fuzz targets: a bounded output
collector plus a standalone replay driver used when the target is not
linked against a fuzzing engine (build with HERBCEPTIONS_FUZZ_ENGINE=ON
to get a -fsanitize=fuzzer binary instead; then any corpus directory /
single inputs work as usual).
*/
#include <herbceptions/error>

#include <cstddef>
#include <cstdint>

namespace herbceptions_fuzz {

inline constexpr ::std::size_t capture_capacity{8192};

struct capture_ctx {
  char buf[capture_capacity];
  ::std::size_t len;
};

inline void reset(capture_ctx &ctx) noexcept {
  ctx.len = 0;
  ctx.buf[0] = '\0';
}

// Collector: appends every reported byte, truncating at capacity. Safe to
// pass to any do_query_information.
inline void capture_cookfun(void *cookie, ::std::io_scatter_t const *v,
                            ::std::size_t n) noexcept {
  auto *ctx{static_cast<capture_ctx *>(cookie)};
  for (::std::size_t i = 0; i < n; ++i) {
    auto const *base{static_cast<char const *>(v[i].base)};
    for (::std::size_t j = 0; j < v[i].len; ++j) {
      if (ctx->len + 1u == capture_capacity) {
        return;
      }
      ctx->buf[ctx->len++] = base[j];
    }
  }
  ctx->buf[ctx->len] = '\0';
}

} // namespace herbceptions_fuzz

#ifndef HERBCEPTIONS_HAVE_FUZZ_ENGINE

#include <cstdio>
#include <cstdint>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const *data,
                                      ::std::size_t size);

int main(int argc, char **argv) {
  ::std::vector<::std::uint8_t> input;
  for (int i = 1; i < argc; ++i) {
    ::std::FILE *f{::std::fopen(argv[i], "rb")};
    if (f == nullptr) {
      ::std::fprintf(stderr, "cannot open %s\n", argv[i]);
      continue;
    }
    input.clear();
    char chunk[4096];
    for (::std::size_t got{};
         (got = ::std::fread(chunk, 1, sizeof(chunk), f)) != 0;) {
      input.insert(input.end(), chunk, chunk + got);
    }
    ::std::fclose(f);
    ::std::fprintf(stderr, "running %s (%zu bytes)\n", argv[i], input.size());
    LLVMFuzzerTestOneInput(input.data(), input.size());
  }
  if (argc == 1) {
    char chunk[4096];
    for (::std::size_t got{};
         (got = ::std::fread(chunk, 1, sizeof(chunk), stdin)) != 0;) {
      input.insert(input.end(), chunk, chunk + got);
    }
    LLVMFuzzerTestOneInput(input.data(), input.size());
  }
  return 0;
}

#endif
