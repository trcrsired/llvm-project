// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx -Wno-return-stack-address -verify %s

// Owner freeze: once a view borrows from an owner, a non-const member call on
// that owner is a call restriction while the view is still live. This replaces
// per-container invalidation lists with a universal liveness-based rule.
// Unlike a lifetime diagnostic, an [[unsafe]] fence permits the call.

struct owner {
  int n = 0;
  void mutate() noexcept { ++n; }
  int read() const noexcept { return n; }
};

[[unsafe]] {
struct view {
  int const* p;
  view(owner const& o [[lifetime("this")]]) noexcept : p(&o.n) {}
};
}

static int bad() noexcept {
  owner o{};
  view v(o); // expected-note {{view borrowing from this object}}
  o.mutate(); // expected-error {{cannot call mutate while a live view borrows from this object}}
  return v.p != nullptr;
}

// A const member does not mutate the owner.
static int good_const() noexcept {
  owner o{};
  view v(o);
  int x = o.read();
  return x + (v.p != nullptr);
}

// Mutating before the view exists is fine.
static void good_before() noexcept {
  owner o{};
  o.mutate();
  view v(o);
  (void)v.p;
}

// The freeze ends at the view's last use, not at the end of the scope.
static int good_after() noexcept {
  owner o{};
  {
    view v(o);
    (void)v.p;
  }
  o.mutate();
  return o.read();
}

// A fence is an explicit escape hatch for this call restriction.
static int good_fence() noexcept {
  owner o{};
  view v(o);
  [[unsafe]] { o.mutate(); }
  return v.p != nullptr;
}

// A live view of a different owner does not freeze this one.
static int good_unrelated() noexcept {
  owner a{};
  owner b{};
  view v(a);
  b.mutate();
  return v.p != nullptr;
}

// Rebinding the view releases the old owner.
static int good_rebind() noexcept {
  owner a{};
  owner b{};
  view v(a);
  (void)v.p;
  v = view(b);
  (void)v.p;
  a.mutate();
  return v.p != nullptr;
}

// Named functions follow the same rule: once frozen, an owner may only be
// passed as `in`, or copied by value. Mutating parameter kinds are rejected.
static void take_inout(inout owner o) noexcept { ++o.n; }
static void take_out(out owner o) noexcept { o = owner{}; }
static void take_move(move owner o) noexcept { ++o.n; }
static void take_forward(forward owner o) noexcept { ++o.n; }
static int take_in(in owner o) noexcept { return o.n; }
static owner copy_it(owner o) noexcept { return o; }

static int bad_free_function() noexcept {
  owner o{};
  view v(o); // expected-note {{view borrowing from this object}}
  take_inout(o); // expected-error {{cannot call take_inout while a live view borrows from this object}}
  return v.p != nullptr;
}

static int bad_move() noexcept {
  owner o{};
  view v(o); // expected-note {{view borrowing from this object}}
  take_move(destructive_move(o)); // expected-error {{cannot call take_move while a live view borrows from this object}}
  return v.p != nullptr;
}

static int good_in_and_copy() noexcept {
  owner o{};
  view v(o);
  int x = take_in(o);
  owner c = copy_it(o);
  (void)c.n;
  return x + (v.p != nullptr);
}

static int good_free_fence() noexcept {
  owner o{};
  view v(o);
  [[unsafe]] { take_forward(o); }
  return v.p != nullptr;
}

int main() { return 0; }
