// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm %s -o - | FileCheck %s

// try() on a `return_failure{E}` call inside a `throws` function converts the
// E error to std::error via error_domain<E>::domain() / code(E). On the
// failure edge the low bytes of the union payload slot hold E, so extracting
// it for code(E) must narrow, not zext: emitting zext produced invalid IR.

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

// i64 success payload, i32 enum error: extract E from the low bytes with a
// trunc and fabricate the {domain, code} pair directly.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z5h_i64Drv()
// CHECK: call { i64, i8 } @_Z7api_i64DESt7my_errcEv()
// CHECK: trunc i64 {{.*}} to i32
// CHECK: call {{.*}}@_ZNSt12error_domainISt7my_errcE4codeES0_(i32
// CHECK: ret { { ptr, i64 }, i8 }
long long api_i64() return_failure{std::my_errc};

long long h_i64() throws { return try(api_i64()); }

// i32 payload, same-width error: no trunc or zext needed at all.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z5h_i32Drv()
// CHECK: call { i32, i8 } @_Z7api_i32DESt7my_errcEv()
// CHECK: ret { { ptr, i64 }, i8 }
int api_i32() return_failure{std::my_errc};

int h_i32() throws { return try(api_i32()); }

// Aggregate payload wider than the error: the fabricated {domain, code} goes
// into the union's low bytes.
// CHECK-LABEL: define dso_local { %struct.Big, i8 } @_Z5h_bigDrv()
// CHECK: call { %struct.Big, i8 } @_Z7api_bigDESt7my_errcEv()
// CHECK: ret { %struct.Big, i8 }
struct Big { long long a, b, c; };
Big api_big() return_failure{std::my_errc};

Big h_big() throws { return try(api_big()); }

// A catch throws(std::error) handler on a return_failure{E} call gets the
// converted error.
// CHECK-LABEL: define dso_local void @_Z7handleri(
// CHECK: {{call|invoke}} { i64, i8 } @_Z7api_i64DESt7my_errcEv()
// CHECK: store %"struct.std::error"
void handler(int x) noexcept {
  try {
    (void)api_i64();
  } catch throws(std::error e) {
    (void)e.code;
  }
}

// CHECK-LABEL: define dso_local noundef i32 @_Z12explicit_tryv()
// CHECK: {{call|invoke}} { i64, i8 } @_Z7api_i64DESt7my_errcEv()
int explicit_try() noexcept {
  try {
    long long v = try(api_i64());
    return (int)v;
  } catch throws(std::error e) {
    return (int)e.code;
  }
}
