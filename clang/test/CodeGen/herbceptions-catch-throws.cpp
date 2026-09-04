// RUN: %clang -std=c++20 -fherbceptions -S -emit-llvm -o - %s | FileCheck %s
// RUN: %clang -std=c++20 -fherbceptions -fno-exceptions %s -o %t
// RUN: %t
// REQUIRES: native

// Herbception `catch throws(E e) { ... }` block handler: a bare call to a
// throws function inside the try block returns {T, i1}, and on failure the
// error value is routed to the handler (which binds the exception variable)
// instead of being silently dropped in a noexcept function.

using size_t = __SIZE_TYPE__;

static int ErrorDestructions;

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
  // The error value is a compiler-fabricated {domain, code} pair whose
  // destructor (which runs the domain's do_cleanup) must execute exactly once
  // when the catch variable goes out of scope, like std::expected.
  ~error() noexcept;
};

enum class errc : unsigned { io_error = 4 };
template <typename T> class error_domain;
template <> class error_domain<errc> {
public:
  static inline unsigned char Domain;
  static void *domain() noexcept { return &Domain; }
  static size_t code(errc) noexcept { return 4; }
};
}

std::error::~error() noexcept { ++ErrorDestructions; }

// CHECK: define dso_local void @_Z6calleem(i64 noundef %{{.*}}) #[[ATTR:[0-9]+]] {
// CHECK: call { { ptr, i64 }, i1 } @_Z3barm
// CHECK: extractvalue { { ptr, i64 }, i1 } %{{.*}}, 1
// CHECK: br i1 %{{.*}}, label %{{.*}}, label %{{.*}}
// CHECK: store %"struct.std::error" %{{.*}}, ptr %{{.*}}, align 8
// CHECK: call void @_ZNSt5errorD1Ev(ptr {{.*}} %{{.*}})
void bar(size_t i) throws {
  if (i == 0) throw throws std::error{nullptr, 4};
}

void callee(size_t i) noexcept {
  try {
    bar(i);
  } catch throws(std::error e) {
    (void)e.code;
  }
}

// A rethrow copies the payload before it relinquishes source ownership. The
// same conditional cleanup remains structurally present but observes false,
// leaving the destination channel as the sole owner.
// CHECK-LABEL: define {{.*}} @_Z15rethrow_failurev()
// CHECK: %herb.catch.owned = alloca i1, align 1
// CHECK: store i1 true, ptr %herb.catch.owned, align 1
// CHECK: call void @llvm.memcpy
// CHECK-NEXT: store i1 false, ptr %herb.catch.owned, align 1
// CHECK: %[[RETHROW_ACTIVE:.*]] = load i1, ptr %herb.catch.owned, align 1
// CHECK: br i1 %[[RETHROW_ACTIVE]], label %cleanup.action, label %cleanup.done
// CHECK: cleanup.action:
// CHECK: call void @_ZNSt5errorD1Ev

// A normal handler exit keeps ownership true, so its conditional cleanup calls
// the destructor. No transfer edge may clear the bit first.
// CHECK-LABEL: define internal {{.*}} @_ZL21consumes_exactly_oncev()
// CHECK: %herb.catch.owned = alloca i1, align 1
// CHECK: store i1 true, ptr %herb.catch.owned, align 1
// CHECK-NOT: store i1 false, ptr %herb.catch.owned
// CHECK: %[[CONSUME_ACTIVE:.*]] = load i1, ptr %herb.catch.owned, align 1
// CHECK: br i1 %[[CONSUME_ACTIVE]], label %cleanup.action, label %cleanup.done
// CHECK: cleanup.action:
// CHECK: call void @_ZNSt5errorD1Ev

// CHECK: attributes #[[ATTR]] = { {{.*}} }

__attribute__((noinline)) void routed_failure() throws {
  throw throws std::errc::io_error;
}

static bool consumes_exactly_once() {
  int Before = ErrorDestructions;
  try {
    routed_failure();
  } catch throws(std::error e) {
    if (e.code != 4)
      return false;
  }
  return ErrorDestructions == Before + 1;
}

__attribute__((noinline)) void rethrow_failure() throws {
  try {
    routed_failure();
  } catch throws(std::error e) {
    (void)e;
    throw throws;
  }
}

static bool rethrow_transfers_ownership() {
  int Before = ErrorDestructions;
  try {
    rethrow_failure();
  } catch throws(std::error e) {
    if (e.code != 4)
      return false;
  }
  return ErrorDestructions == Before + 1;
}

int main() {
  // Handler ownership is linear. A normally completed handler consumes its
  // caught value once. A rethrow transfers that same ownership to the outer
  // handler, so the inner cleanup is inactive and the outer cleanup remains
  // the single destructor invocation for the failure.
  if (!consumes_exactly_once())
    return 1;
  if (!rethrow_transfers_ownership())
    return 2;
  return ErrorDestructions == 2 ? 0 : 3;
}
