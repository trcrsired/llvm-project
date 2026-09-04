// RUN: %clang_cc1 -std=c++20 -fherbceptions -fsyntax-only -verify %s
// expected-no-diagnostics

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
};
}

template <class T> T &&declval() noexcept;

struct Active {
  Active() throws;
  Active(const Active &) throws;
  Active(Active &&) throws;
  Active &operator=(const Active &) throws;
  Active &operator=(Active &&) throws;
};

struct Legacy {
  Legacy() noexcept(false);
  Legacy(const Legacy &) noexcept(false);
  Legacy(Legacy &&) noexcept(false);
  Legacy &operator=(const Legacy &) noexcept(false);
  Legacy &operator=(Legacy &&) noexcept(false);
};

struct Disabled {
  Disabled() throws(false);
  Disabled(const Disabled &) throws(false);
  Disabled(Disabled &&) throws(false);
  Disabled &operator=(const Disabled &) throws(false);
  Disabled &operator=(Disabled &&) throws(false);
};

#define DEFAULT_ALL(Type)                                                      \
  Type() = default;                                                            \
  Type(const Type &) = default;                                                \
  Type(Type &&) = default;                                                     \
  Type &operator=(const Type &) = default;                                     \
  Type &operator=(Type &&) = default

struct ActiveThenLegacy {
  DEFAULT_ALL(ActiveThenLegacy);
  Active active;
  Legacy legacy;
};

struct LegacyThenActive {
  DEFAULT_ALL(LegacyThenActive);
  Legacy legacy;
  Active active;
};

struct LegacyOnly {
  DEFAULT_ALL(LegacyOnly);
  Legacy legacy;
};

struct DisabledOnly {
  DEFAULT_ALL(DisabledOnly);
  Disabled disabled;
};

int active_initializer() throws;
int legacy_initializer() noexcept(false);

struct ActiveInitializer {
  ActiveInitializer() = default;
  int value = active_initializer();
};

struct MixedInitializer {
  MixedInitializer() = default;
  int first = legacy_initializer();
  int second = active_initializer();
};

// A live deterministic subobject operation selects the generated `throws`
// type for every constructor and assignment form.  Reversing the member order
// cannot allow a legacy noexcept(false) operation to erase that channel.
static_assert(throws(ActiveThenLegacy{}));
static_assert(throws(LegacyThenActive{}));
static_assert(throws(ActiveThenLegacy(declval<const ActiveThenLegacy &>())));
static_assert(throws(LegacyThenActive(declval<const LegacyThenActive &>())));
static_assert(throws(ActiveThenLegacy(declval<ActiveThenLegacy &&>())));
static_assert(throws(LegacyThenActive(declval<LegacyThenActive &&>())));
static_assert(throws(declval<ActiveThenLegacy &>() =
                     declval<const ActiveThenLegacy &>()));
static_assert(throws(declval<LegacyThenActive &>() =
                     declval<const LegacyThenActive &>()));
static_assert(throws(declval<ActiveThenLegacy &>() =
                     declval<ActiveThenLegacy &&>()));
static_assert(throws(declval<LegacyThenActive &>() =
                     declval<LegacyThenActive &&>()));

static_assert(__has_herbceptions_throws_constructor(ActiveThenLegacy));
static_assert(__has_herbceptions_throws_constructor(LegacyThenActive));
static_assert(__has_herbceptions_throws_copy(ActiveThenLegacy));
static_assert(__has_herbceptions_throws_copy(LegacyThenActive));
static_assert(__has_herbceptions_throws_assign(ActiveThenLegacy));
static_assert(__has_herbceptions_throws_assign(LegacyThenActive));
static_assert(__has_herbceptions_throws_move_assign(ActiveThenLegacy));
static_assert(__has_herbceptions_throws_move_assign(LegacyThenActive));

// Default member initializers use CalledStmt rather than CalledDecl.  Both a
// pure deterministic tree and a mixed deterministic/legacy tree must infer a
// live channel.
static_assert(throws(ActiveInitializer{}));
static_assert(throws(MixedInitializer{}));
static_assert(__has_herbceptions_throws_constructor(ActiveInitializer));
static_assert(__has_herbceptions_throws_constructor(MixedInitializer));

// A legacy-only generated operation remains noexcept(false), while the false
// conditional spelling is ordinary noexcept and never creates a shaped ABI.
static_assert(!throws(LegacyOnly{}));
static_assert(!noexcept(LegacyOnly{}));
static_assert(!throws(declval<LegacyOnly &>() =
                      declval<const LegacyOnly &>()));
static_assert(!noexcept(declval<LegacyOnly &>() =
                        declval<const LegacyOnly &>()));
static_assert(!__has_herbceptions_throws_constructor(LegacyOnly));
static_assert(!__has_herbceptions_throws_assign(LegacyOnly));

static_assert(!throws(DisabledOnly{}));
static_assert(noexcept(DisabledOnly{}));
static_assert(!throws(declval<DisabledOnly &>() =
                      declval<const DisabledOnly &>()));
static_assert(noexcept(declval<DisabledOnly &>() =
                       declval<const DisabledOnly &>()));
static_assert(!__has_herbceptions_throws_constructor(DisabledOnly));
static_assert(!__has_herbceptions_throws_assign(DisabledOnly));

#undef DEFAULT_ALL
