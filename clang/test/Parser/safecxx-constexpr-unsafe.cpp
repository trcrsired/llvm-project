// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx -verify %s

// Constant evaluation happens at compile time, so [[unsafe]] call restrictions
// do not apply inside a constexpr/constinit initializer, a static_assert, or an
// immediate function. Runtime calls still need a fence.

[[unsafe]] static constexpr int compile_time() noexcept { return 42; }
[[unsafe]] static int at_runtime() noexcept { return 7; }

struct [[unsafe]] Box {
  constexpr Box(int n) noexcept : n(n) {}
  int n;
};

constexpr int a = compile_time();
constexpr Box b{1};
constinit int c = compile_time();
static_assert(compile_time() == 42);

static void runtime_source() noexcept {
  constexpr int x = compile_time();
  constexpr Box y{2};
  (void)x;
  (void)y;
  (void)at_runtime(); // expected-error {{call to 'at_runtime' is unsafe}}
  Box z{3}; // expected-error {{call to 'Box' is unsafe}}
}

[[unsafe]] {
static void fenced() noexcept {
  (void)at_runtime();
  Box z{4};
}
}
