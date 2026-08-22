#pragma once
#if __has_include(<math.h>)
#include <math.h>
#endif

namespace std {
enum class cmath_errc : ::std::uint_least32_t {
#ifdef FE_INVALID
  divbyzero = FE_DIVBYZERO,
  inexact = FE_INEXACT,
  invalid = FE_INVALID,
  overflow = FE_OVERFLOW,
  underflow = FE_UNDERFLOW,
#else
  divbyzero = 1,
  inexact = 2,
  invalid = 4,
  overflow = 8,
  underflow = 16,
#endif
  all_except = divbyzero | inexact | invalid | overflow | underflow
};
} // namespace std
