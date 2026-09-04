// RUN: %clang_cc1 -std=c++20 -fherbceptions -fsyntax-only -verify %s

template <class, class> struct __herb_catch_fails {};

#define OCCUPY_FALLBACK(N)                                                     \
  template <class, class> struct __clang_herb_catch_fails_fallback_##N {};

OCCUPY_FALLBACK(1)
struct __clang_herb_catch_fails_fallback_2 {};
OCCUPY_FALLBACK(3)
int __clang_herb_catch_fails_fallback_4;
OCCUPY_FALLBACK(5)
void __clang_herb_catch_fails_fallback_6();
OCCUPY_FALLBACK(7)
OCCUPY_FALLBACK(8)

int collision_source(bool fail) return_failure{int} {
  if (fail)
    return_failure 31;
  return 37;
}

int inspect_collision_result(bool fail) {
  auto result = catch return_failure(collision_source(fail));
  return result.failed ? result.error : result.value;
}

// expected-no-diagnostics
