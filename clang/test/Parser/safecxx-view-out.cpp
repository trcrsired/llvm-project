// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx -Wno-return-stack-address -verify %s

// A view written to a view-typed `out`/`inout` parameter is an output the
// caller keeps using after the call. Its provenance must come from an input:
// a view, `this`, or a caller-owned owner. Callee-local storage and
// callee-owned copies are rejected.

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

// Legit: the output borrows from a caller-owned owner input.
static void good(out view s, inout owner v) noexcept { s = view(v); }

// Legit: the output borrows from another view input.
static void good_view(out view s, view v) noexcept { s = v; }

// Legit: an `inout` view may be passed through.
static void good_roundtrip(inout view s) noexcept {
  view t = s;
  s = t;
}

// Callee-local owner.
// expected-note@+1 {{escapes through output parameter 's'}}
static void bad_local(out view s) noexcept {
  owner local{};
  // expected-error@+1 {{stack memory associated with local variable 'local' is written to output parameter 's'}}
  s = view(local);
}

// A by-value owner is a callee-owned copy, not an input.
// expected-note@+1 {{escapes through output parameter 's'}}
static void bad_value(out view s, owner v) noexcept {
  // expected-error@+1 {{stack memory associated with parameter 'v' is written to output parameter 's'}}
  s = view(v);
}

// `move` also transfers ownership to the callee. A placeholder loan has no
// write expression, so the diagnostic names the parameter itself.
// expected-error@+2 {{stack memory associated with parameter 'v' is written to output parameter 's'}}
// expected-note@+1 {{escapes through output parameter 's'}}
static void bad_move(out view s, move owner v) noexcept {
  s = view(v);
}

// A local view still carries the borrow of the local owner behind it.
// expected-note@+1 {{escapes through output parameter 's'}}
static void bad_nested(out view s) noexcept {
  owner local{};
  // expected-error@+1 {{stack memory associated with local variable 'local' is written to output parameter 's'}}
  view v(local);
  s = v;
}

// The caller's use is fine when the owner outlives the view.
static int use() noexcept {
  owner o{};
  view s;
  good(s, o);
  return s.p != nullptr;
}

// The caller's view inherits the borrows of the inputs, so using it after the
// input owner dies is diagnosed at the use.
static int caller_bad() noexcept {
  view s;
  {
    owner v{};
    // expected-error@+1 {{local variable 'v' does not live long enough}}
    good(s, v);
  } // expected-note {{local variable 'v' is destroyed here}}
  // expected-note@+1 {{later used here}}
  return s.p != nullptr;
}

int main() { return use(); }
