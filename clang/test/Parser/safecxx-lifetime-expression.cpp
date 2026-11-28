// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx -verify %s

// [[lifetime_expression]] marks a class whose objects may only exist as
// temporaries inside a larger expression. It is independent of [[unsafe]]
// (which is a fence / call restriction) and of the lifetime protocol marks.

struct [[lifetime_expression]] expr_only {
  int x;
};
struct plain {
  int x;
};

// A parameter or reference is not a stored object.
static void consume(expr_only) noexcept {}
static void consume_ref(in expr_only) noexcept {}

static void temporaries() noexcept {
  consume(expr_only{});
  consume_ref(expr_only{});
  (void)plain{};
}

static void stored() noexcept {
  expr_only a; // expected-error {{expression-only}}
  (void)a;
}

struct field_holder {
  expr_only e; // expected-error {{expression-only}}
};

[[unsafe]] {
static void fenced() noexcept {
  expr_only a; // OK inside a fence
  (void)a;
}
}
