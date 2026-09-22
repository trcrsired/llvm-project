// RUN: %clang -std=c++20 -fherbceptions -fno-exceptions -S -fdiscard-value-names -emit-llvm -o - %s | FileCheck %s
// RUN: %clang -std=c++20 -fherbceptions -fno-exceptions -c -o /dev/null %s

// try() on a `return_failure{E}` call inside a `throws` function converts the E error
// to std::error via error_domain<E>::domain() / code(E). The callee's union
// payload slot is read as the union type (max(T, E)), but on the failure edge
// its low bytes hold E -- the value passed to code(E) must be extracted as E.
// When T is wider than E (e.g. i64 T vs i32 enum E) that extraction is a
// trunc, not a zext: emitting zext i64 -> i32 produced invalid IR and crashed
// instruction selection (Cannot select: i32 = zero_extend).

namespace std {
struct error_domain_singleton {};
struct error {
  void *d;
  __SIZE_TYPE__ code;
};
enum class my_errc : int { bad = 7 };
template <class T> class error_domain;
template <> class error_domain<my_errc> {
public:
  static error_domain_singleton const *domain() noexcept;
  static __SIZE_TYPE__ code(my_errc e) noexcept { return (__SIZE_TYPE__)e; }
};
}

// i64 success payload, i32 enum error: the error edge extracts E from the
// low bytes of the i64 slot with a trunc, then returns the fabricated
// {domain, code} pair directly -- no round trip through the return slot.
//
// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z5h_i64v(
// CHECK:         call { i64, i1 } @_Z7api_i64v
// CHECK:         br i1 %{{.*}}, label %[[ERR:[0-9]+]], label %[[OK:[0-9]+]]
// CHECK:       [[ERR]]:
// CHECK-NEXT:    trunc i64 %{{.*}} to i32
// CHECK-NEXT:    [[DOMAIN:call noundef ptr]]
// CHECK:         call {{.*}}code
// CHECK:         insertvalue { ptr, i64 } {{.*}}
// CHECK:         insertvalue { { ptr, i64 }, i1 } {{.*}}, i1 true
// CHECK:         ret { { ptr, i64 }, i1 }
long long api_i64() return_failure{std::my_errc};

long long h_i64() throws { return try(api_i64()); }

// i32 payload, same-width error: no trunc or zext needed at all.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z5h_i32v(
// CHECK-NOT:     zext i64
// CHECK:         call {{.*}}code
// CHECK:         ret { { ptr, i64 }, i1 }
int api_i32() return_failure{std::my_errc};

int h_i32() throws { return try(api_i32()); }

// Aggregate payload wider than the error: the fabricated {domain, code} goes
// into the union's low bytes with the tail zeroed.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z5h_bigv(
// CHECK:         call {{.*}}code
// CHECK:         ret { { ptr, i64 }, i1 }
struct Big { long long a, b, c; };
Big api_big() return_failure{std::my_errc};

Big h_big() throws { return try(api_big()); }

// A catch throws(std::error) handler on a return_failure{E} call gets the converted
// std::error, the same conversion auto-propagation performs.
// CHECK-LABEL: define dso_local void @_Z7handleri(
// CHECK:         call { i64, i1 } @_Z7api_i64v
// CHECK:         trunc i64 %{{.*}} to i32
// CHECK:         call {{.*}}code
// CHECK:         store %"struct.std::error" %{{.*}}, ptr %{{.*}}, align 8
void handler(int x) noexcept {
  try {
    (void)api_i64();
  } catch throws(std::error e) {
    (void)e.code;
  }
}

// An explicit try(expr) inside a `try { }` body is valid in a non-throws
// function too: the error routes to the catch throws handler, with the same
// return_failure{E} -> std::error conversion.
// CHECK-LABEL: define dso_local noundef i32 @_Z12explicit_tryv(
// CHECK:         call { i64, i1 } @_Z7api_i64v
// CHECK:         trunc i64 %{{.*}} to i32
// CHECK:         call {{.*}}code
// CHECK:         store %"struct.std::error" %{{.*}}, ptr %{{.*}}, align 8
int explicit_try() noexcept {
  try {
    long long v = try(api_i64());
    return (int)v;
  } catch throws(std::error e) {
    return (int)e.code;
  }
}
