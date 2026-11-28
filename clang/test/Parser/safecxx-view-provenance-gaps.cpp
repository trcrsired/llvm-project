// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx -Wno-return-stack-address -verify %s

// Three provenance gaps:
//  1. a view nested two levels deep in an aggregate;
//  2. a user-defined copy of a view holder;
//  3. a const method that mutates through `mutable` state.
// All must freeze the owner while a live view borrows from it.

[[unsafe]] {
struct [[owner]] owner {
  char *p;
  mutable int touched;
  void mutate() noexcept {}
  void mutate_const() const noexcept { ++touched; }
};
struct view {
  char const *p;
  view() noexcept : p(nullptr) {}
  view(owner const &o [[lifetime("this")]]) noexcept : p(o.p) {}
};
struct holder_uc {
  view v;
  holder_uc() noexcept {}
  holder_uc(holder_uc const &o) noexcept : v(o.v) {} // user-defined copy
};
} // end trusted-unsafe library implementation

struct holder {
  view v;
};
struct nested {
  holder h;
};
static bool nested_freeze() noexcept {
  owner o{};
  nested n;
  n.h.v = view(o); // expected-note {{view borrowing from this object}}
  o.mutate(); // expected-error {{cannot call mutate while a live view borrows from this object}}
  return n.h.v.p != nullptr; // keep the view live past the call
}

static bool user_copy_freeze() noexcept {
  owner o{};
  holder_uc a;
  a.v = view(o); // expected-note {{view borrowing from this object}}
  holder_uc b = a;
  o.mutate(); // expected-error {{cannot call mutate while a live view borrows from this object}}
  return b.v.p != nullptr;
}

static bool mutable_const_freeze() noexcept {
  owner o{};
  view v(o); // expected-note {{view borrowing from this object}}
  o.mutate_const(); // expected-error {{cannot call mutate_const while a live view borrows from this object}}
  return v.p != nullptr;
}
