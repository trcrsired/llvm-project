// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx -Wno-return-stack-address -verify %s

// A class that contains a view is itself a view. Its provenance is the union
// of the provenance of its view-typed fields, so returning or writing such an
// aggregate must be checked just like returning or writing a view directly.
// Raw pointer members are opaque in safe source and are deliberately not
// modelled.

[[unsafe]] {
struct owner {
  char* p;
};
struct view {
  char const* p;
  view() noexcept : p(nullptr) {}
  view(owner const& o [[lifetime("this")]]) noexcept : p(o.p) {}
};
} // end trusted-unsafe library implementation

struct holder {
  int n;
  view v;
};
struct pair_of_views {
  view a;
  view b;
};
struct wrapper {
  view v;
};

// Multi-member aggregate initialization: the second element carries the
// borrow, even though the first is an ordinary int.
static holder two() noexcept {
  owner o{};
  return holder{0, view(o)}; // expected-error {{stack memory associated with local variable 'o' is returned}} expected-note {{returned here}}
}

static pair_of_views three() noexcept {
  owner o{};
  return pair_of_views{view(o), view(o)}; // expected-error {{stack memory associated with local variable 'o' is returned}} expected-error {{stack memory associated with local variable 'o' is returned}} expected-note {{returned here}} expected-note {{returned here}}
}

static holder two_local() noexcept {
  owner o{};
  holder h{0, view(o)}; // expected-error {{stack memory associated with local variable 'o' is returned}}
  return h; // expected-note {{returned here}}
}

static holder field_assign() noexcept {
  owner o{};
  holder h{};
  h.v = view(o); // expected-error {{stack memory associated with local variable 'o' is returned}}
  return h; // expected-note {{returned here}}
}

static void out_agg(out holder h) noexcept { // expected-note {{escapes through output parameter 'h'}}
  owner o{};
  h = holder{0, view(o)}; // expected-error {{stack memory associated with local variable 'o' is written to output parameter 'h'}}
}

static void out_field(out holder h) noexcept { // expected-note {{escapes through output parameter 'h'}}
  owner o{};
  h = holder{};
  h.v = view(o); // expected-error {{stack memory associated with local variable 'o' is written to output parameter 'h'}}
}

// The view stored in a local wrapper is still a borrow of its local owner.
static void read_field(out view s) noexcept { // expected-note {{escapes through output parameter 's'}}
  owner o{};
  wrapper w{};
  w.v = view(o); // expected-error {{stack memory associated with local variable 'o' is written to output parameter 's'}}
  s = w.v;
}

// Legit: view fields borrow from caller-owned owners.
static holder good(inout owner o) noexcept { return holder{0, view(o)}; }
static void good_roundtrip(inout holder h) noexcept {
  holder t = h;
  h = t;
}
static void good_read(out view s, inout wrapper w) noexcept { s = w.v; }

// The caller's fields inherit the input borrows, so using them after the
// owner dies is diagnosed at the use.
static void get(out holder h, inout owner o) noexcept { h = holder{0, view(o)}; }
static int caller_bad() noexcept {
  holder h;
  {
    owner o{};
    get(h, o); // expected-error {{local variable 'o' does not live long enough}}
  } // expected-note {{local variable 'o' is destroyed here}}
  return h.v.p != nullptr; // expected-note {{later used here}}
}

int main() { return 0; }
