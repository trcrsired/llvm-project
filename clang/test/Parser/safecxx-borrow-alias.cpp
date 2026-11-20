// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// `inout auto` lowers to a reference, so it aliases the source's storage.
// The range-for freeze and the destructive_move use-after-move check must
// follow that alias instead of only the declaration named in the source.

[[unsafe]] {
struct It {
  int x = 0;
  int &operator*() noexcept { return x; }
  It &operator++() noexcept { return *this; }
  bool operator!=(const It &) const noexcept { return false; }
};
struct V {
  [[unsafe]] It begin() noexcept { return {}; }
  [[unsafe]] It end() noexcept { return {}; }
};
}

struct Buf {
  int x;
};

static void consume(destructive_move Buf b) noexcept {}
static void use(in V) noexcept {}

static void uam_alias() noexcept {
  Buf a{1};
  inout auto alias = a;
  consume(destructive_move(a));
  alias.x = 2; // expected-error {{value 'alias' is used after it was passed to a move parameter}}
}

static void freeze_alias() noexcept {
  V v;
  inout auto alias = v;
  for (in auto x : v) {
    (void)x;
    use(alias); // expected-error {{cannot use 'alias' inside a range-for over it}}
  }
}
