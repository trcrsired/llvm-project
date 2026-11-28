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
  o.mutate(); // expected-error {{cannot call non-const member function mutate while a live view borrows from this object}}
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

int main() { return 0; }
