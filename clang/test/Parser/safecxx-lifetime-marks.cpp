// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx -verify %s

// P1179 public provenance marks. The compiler translates [[owner]] and
// [[lifetime("this")]] to its internal lifetime engine under the toggle;
// source never spells the Clang attributes.

struct Vec { int *p; };

// Mark 1: pointer fields are owned by the object.
class [[owner]] Owned {
  int *p;
};

struct View {
  int *ptr;

  // Mark 2 on a parameter: the view's pointer derives from that argument.
  template <class R>
  View(forward R r [[lifetime("this")]]) noexcept : ptr(r.p) {}

  // Mark 2 on the implicit object parameter: a view produced by a member
  // borrows from the object it was cut from.
  View sub() const noexcept [[lifetime("this")]] { return *this; }
};

static View escape_ctor() noexcept {
  Vec v{};
  // expected-warning@+1 {{address of stack memory associated with local variable 'v' returned}}
  return View(v); // expected-error {{stack memory associated with local variable 'v' is returned}}
  // expected-note@-1 {{returned here}}
}

static View escape_member() noexcept {
  Vec v{};
  // The escaped view is a transparent alias for its owner, so the dangling
  // borrow is attributed to 'v', the owner that actually dies.
  // expected-error@+1 {{stack memory associated with local variable 'v' is returned}}
  View s(v);
  // expected-warning@+1 {{address of stack memory associated with local variable 's' returned}}
  return s.sub(); // expected-note {{returned here}}
}
