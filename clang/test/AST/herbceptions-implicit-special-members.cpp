// RUN: %clang_cc1 -std=c++20 -fherbceptions -ast-dump %s | FileCheck %s

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

struct ActiveThenLegacy {
  ActiveThenLegacy() = default;
  ActiveThenLegacy(const ActiveThenLegacy &) = default;
  ActiveThenLegacy(ActiveThenLegacy &&) = default;
  ActiveThenLegacy &operator=(const ActiveThenLegacy &) = default;
  ActiveThenLegacy &operator=(ActiveThenLegacy &&) = default;
  Active active;
  Legacy legacy;
};

struct LegacyThenActive {
  LegacyThenActive() = default;
  LegacyThenActive(const LegacyThenActive &) = default;
  LegacyThenActive(LegacyThenActive &&) = default;
  LegacyThenActive &operator=(const LegacyThenActive &) = default;
  LegacyThenActive &operator=(LegacyThenActive &&) = default;
  Legacy legacy;
  Active active;
};

struct LegacyOnly {
  LegacyOnly() = default;
  LegacyOnly(const LegacyOnly &) = default;
  LegacyOnly &operator=(const LegacyOnly &) = default;
  Legacy legacy;
};

int active_initializer() throws;
int legacy_initializer() noexcept(false);

struct ActiveThenLegacyInitializer {
  ActiveThenLegacyInitializer() = default;
  int active = active_initializer();
  int legacy = legacy_initializer();
};

struct LegacyThenActiveInitializer {
  LegacyThenActiveInitializer() = default;
  int legacy = legacy_initializer();
  int active = active_initializer();
};

// Force every lazy exception specification before dumping the AST.
static_assert(throws(ActiveThenLegacy{}));
static_assert(throws(ActiveThenLegacy(declval<const ActiveThenLegacy &>())));
static_assert(throws(ActiveThenLegacy(declval<ActiveThenLegacy &&>())));
static_assert(throws(declval<ActiveThenLegacy &>() =
                     declval<const ActiveThenLegacy &>()));
static_assert(throws(declval<ActiveThenLegacy &>() =
                     declval<ActiveThenLegacy &&>()));
static_assert(throws(LegacyThenActive{}));
static_assert(throws(LegacyThenActive(declval<const LegacyThenActive &>())));
static_assert(throws(LegacyThenActive(declval<LegacyThenActive &&>())));
static_assert(throws(declval<LegacyThenActive &>() =
                     declval<const LegacyThenActive &>()));
static_assert(throws(declval<LegacyThenActive &>() =
                     declval<LegacyThenActive &&>()));
static_assert(!noexcept(LegacyOnly{}));
static_assert(!noexcept(LegacyOnly(declval<const LegacyOnly &>())));
static_assert(!noexcept(declval<LegacyOnly &>() =
                        declval<const LegacyOnly &>()));
static_assert(throws(ActiveThenLegacyInitializer{}));
static_assert(throws(LegacyThenActiveInitializer{}));

// CHECK-LABEL: CXXRecordDecl {{.*}} struct ActiveThenLegacy definition
// CHECK: CXXConstructorDecl {{.*}} ActiveThenLegacy 'void () throws'
// CHECK: CXXConstructorDecl {{.*}} ActiveThenLegacy 'void (const ActiveThenLegacy &) throws'
// CHECK: CXXConstructorDecl {{.*}} ActiveThenLegacy 'void (ActiveThenLegacy &&) throws'
// CHECK: CXXMethodDecl {{.*}} operator= 'ActiveThenLegacy &(const ActiveThenLegacy &) throws'
// CHECK: CXXMethodDecl {{.*}} operator= 'ActiveThenLegacy &(ActiveThenLegacy &&) throws'
// CHECK-LABEL: CXXRecordDecl {{.*}} struct LegacyThenActive definition
// CHECK: CXXConstructorDecl {{.*}} LegacyThenActive 'void () throws'
// CHECK: CXXConstructorDecl {{.*}} LegacyThenActive 'void (const LegacyThenActive &) throws'
// CHECK: CXXConstructorDecl {{.*}} LegacyThenActive 'void (LegacyThenActive &&) throws'
// CHECK: CXXMethodDecl {{.*}} operator= 'LegacyThenActive &(const LegacyThenActive &) throws'
// CHECK: CXXMethodDecl {{.*}} operator= 'LegacyThenActive &(LegacyThenActive &&) throws'
// CHECK-LABEL: CXXRecordDecl {{.*}} struct LegacyOnly definition
// CHECK: CXXConstructorDecl {{.*}} LegacyOnly 'void () noexcept(false)'
// CHECK: CXXConstructorDecl {{.*}} LegacyOnly 'void (const LegacyOnly &) noexcept(false)'
// CHECK: CXXMethodDecl {{.*}} operator= 'LegacyOnly &(const LegacyOnly &) noexcept(false)'
// CHECK-LABEL: CXXRecordDecl {{.*}} struct ActiveThenLegacyInitializer definition
// CHECK: CXXConstructorDecl {{.*}} ActiveThenLegacyInitializer 'void () throws'
// CHECK-LABEL: CXXRecordDecl {{.*}} struct LegacyThenActiveInitializer definition
// CHECK: CXXConstructorDecl {{.*}} LegacyThenActiveInitializer 'void () throws'
