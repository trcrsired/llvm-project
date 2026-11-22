// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// [[unsafe]] on a data member is an access restriction: the member may only be
// written from unsafe context. Aggregate initialization writes it without
// building a member reference (positional), or names it only in a designator,
// so the member-access check cannot see it. It is checked here instead.
//
// The escape is the same one the attribute always offers: initialize inside a
// [[unsafe]] block, or mark the declared object itself [[unsafe]].

struct Members {
  [[unsafe]] int hidden;
  int plain;
};

static void use(in int) noexcept {}

// Positional aggregate initialization. The first element writes `hidden`.
static void positional() noexcept {
  Members m{1, 2}; // expected-error {{data member 'hidden' is declared [[unsafe]]}}
  use(m.plain);
}

// Designated initialization names the member; still a write.
static void designated() noexcept {
  Members m{.hidden = 1, .plain = 2}; // expected-error {{data member 'hidden' is declared [[unsafe]]}}
  use(m.plain);
}

// Copy-list-initialization takes the same path.
static void copy_list() noexcept {
  Members m = {3, 4}; // expected-error {{data member 'hidden' is declared [[unsafe]]}}
  use(m.plain);
}

// An empty list value-initializes the omitted member, which is a write.
static void empty_list() noexcept {
  Members m{}; // expected-error {{data member 'hidden' is declared [[unsafe]]}}
  use(m.plain);
}

// A partial list that omits the member still writes it.
static void partial() noexcept {
  Members m{1}; // expected-error {{data member 'hidden' is declared [[unsafe]]}}
  use(m.plain);
}

// The same rule applies at namespace scope.
static Members global{1, 2}; // expected-error {{data member 'hidden' is declared [[unsafe]]}}

// ... and through a nested aggregate.
struct Outer {
  Members m;
  int tag;
};
static void nested() noexcept {
  Outer o{{1, 2}, 3}; // expected-error {{data member 'hidden' is declared [[unsafe]]}}
  use(o.tag);
}

// ... and through an array of aggregates.
static void array() noexcept {
  Members a[2] = {{1, 2}, {3, 4}}; // expected-error 2 {{data member 'hidden' is declared [[unsafe]]}}
  use(a[0].plain);
}

// ... and at template instantiation.
template <class T> static void make() noexcept {
  T t{1, 2}; // expected-error {{data member 'hidden' is declared [[unsafe]]}}
  use(t.plain);
}
static void instantiate() noexcept { make<Members>(); } // expected-note {{in instantiation of function template specialization 'make<Members>'}}

// Escapes. The declaration mark covers the initializer only.
static void declaration_mark() noexcept {
  [[unsafe]] Members m{1, 2};
  use(m.plain);
}

// A statement fence covers the initialization where it is written.
static void statement_fence() noexcept {
  Members m;
  [[unsafe]] { m = Members{1, 2}; }
  use(m.plain);
}

// Nothing is written: default-initialization and unevaluated uses stay legal.
static void no_initializer() noexcept {
  Members m;
  use(m.plain);
}
static void unevaluated(inout Members m) noexcept {
  (void)sizeof(m.hidden);
}

// A default member initializer belongs to the member's own declaration and is
// already sanctioned by the attribute on the member.
struct Defaulted {
  [[unsafe]] int hidden = 0;
  int plain = 1;
};
static void default_member() noexcept {
  Defaulted d;
  use(d.plain);
}

// ---------------------------------------------------------------------------
// Whole-object operations. The synthesized copy/move body mentions `hidden` on
// the class author's behalf, so the operation is judged at each use site.
// ---------------------------------------------------------------------------

static void copy_rejected(inout Members a) noexcept {
  Members b(a); // expected-error {{implicit copy or move of 'Members' is not allowed in safe C++ source}}
  use(b.plain);
}

static void assign_rejected(inout Members a, inout Members b) noexcept {
  b = a; // expected-error {{implicit copy or move of 'Members' is not allowed in safe C++ source}}
}

static void binding_rejected(inout Members a) noexcept {
  auto [h, p] = a; // expected-error {{implicit copy or move of 'Members' is not allowed in safe C++ source}}
  use(h);
  use(p);
}

static void whole_object_fenced(inout Members a) noexcept {
  [[unsafe]] {
    Members b(a);
    b = a;
    auto [h, p] = a;
    use(h);
    use(p);
    use(b.plain);
  }
}

// A class without an [[unsafe]] member is unaffected.
struct Plain {
  int x;
  int y;
};
static void plain_copy(inout Plain a) noexcept {
  Plain b(a);
  b = a;
  use(b.x);
}
