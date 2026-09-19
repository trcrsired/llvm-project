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
