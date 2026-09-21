// RUN: %clang_cc1 -std=c++26 -fherbceptions -emit-obj -o /dev/null -verify %s

// Dependent throws(cond): a value-dependent condition must be preserved in the
// function type until instantiation (EST_DependentThrows), then evaluated:
//   throws(true)  -> throws
//   throws(false) -> noexcept

// Concrete conditions normalize immediately.
void t_true() throws(true);
void t_false() throws(false);

static_assert(throws(t_true()), "throws(true) should be throws");
static_assert(!throws(t_false()), "throws(false) should be noexcept");
static_assert(!noexcept(t_true()), "throws(true) is not noexcept");
static_assert(noexcept(t_false()), "throws(false) is noexcept");

// Dependent condition on a template parameter: the spec stays dependent until
// instantiation, then resolves like a concrete condition.
template <bool B>
void f() throws(B);

static_assert(throws(f<true>()), "f<true> should be throws");
static_assert(!throws(f<false>()), "f<false> should be noexcept");
static_assert(noexcept(f<false>()), "f<false> should be noexcept");

// Call-site legality follows the resolved spec.
void ok() throws {
  f<true>();
  f<false>();
}

void bad() noexcept {
  f<false>(); // fine: f<false> is noexcept
  f<true>();  // expected-error {{call to 'throws' function in a non-'throws' function must be handled by an enclosing 'try { } catch throws' block, or mark the calling function as 'throws' so herbceptions can propagate}}
}

// The conditional-forwarding pattern: propagate the callee's spec through a
// dependent 'throws(!noexcept(call))' specification.
template <typename T>
struct may_throw {
  static void go() throws;
};

template <typename T>
struct no_throw {
  static void go() noexcept;
};

template <typename S>
void call_go() throws(!noexcept(S::go()));

static_assert(throws(call_go<may_throw<int>>()), "forwarded spec should be throws");
static_assert(!throws(call_go<no_throw<int>>()), "forwarded spec should be noexcept");
static_assert(noexcept(call_go<no_throw<int>>()), "forwarded spec should be noexcept");

// Dependent conditions may also contain unexpanded parameter packs.
template <typename... Ts>
void all_throw() throws((!noexcept(Ts::go()) || ...));

static_assert(throws(all_throw<no_throw<int>, may_throw<char>>()),
              "pack with a throws member should be throws");
static_assert(!throws(all_throw<no_throw<int>, no_throw<char>>()),
              "pack of noexcept members should be noexcept");

// Member function templates instantiated from an enclosing class template.
template <bool B>
struct holder {
  void m() throws(B);
};

static_assert(throws(holder<true>{}.m()), "holder<true>::m should be throws");
static_assert(!throws(holder<false>{}.m()), "holder<false>::m should be noexcept");

// A member of a class template calling another throws member through this->:
// the implicit try() must be decided against the spec as resolved at
// instantiation, not the dependent spec in the pattern.
template <typename T>
struct member_calls {
  void callee() throws {}

  // Dependent spec resolving to noexcept: calling a throws member is
  // ill-formed, and the diagnostic must name the real problem rather than
  // complain about a stale implicit try().
  void run_noexcept() throws(sizeof(T) > 100) {
    this->callee(); // expected-error {{call to 'throws' function in a non-'throws' function must be handled by an enclosing 'try { } catch throws' block, or mark the calling function as 'throws' so herbceptions can propagate}}
  }

  // Dependent spec resolving to throws: propagates.
  void run_throws() throws(sizeof(T) >= 1) { this->callee(); }
};

// throws caller + dependent-spec callee resolving to noexcept: a plain call.
template <typename T>
struct member_dep_callee {
  void maybe() throws(sizeof(T) > 100) {}
  void run() throws { this->maybe(); }
};

// Member function template of a class template.
template <typename T>
struct member_fn_tmpl {
  void callee() throws {}
  template <typename I>
  void run(I first, I last) throws { this->callee(); }
};

void use_member_calls() throws {
  member_calls<int> mc;
  mc.run_noexcept(); // expected-note {{in instantiation of member function 'member_calls<int>::run_noexcept' requested here}}
  mc.run_throws();

  member_dep_callee<int> mdc;
  mdc.run();

  member_fn_tmpl<int> mft;
  int a[1] = {};
  mft.run(a, a + 1);
}
