// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=OGCG

// `auto` return type deduction on a throws function: the deduced return type
// is the payload type while the ABI signature carries the herbception
// {union{T, std::error}, i8} pair. A bare `throw throws;` inside the
// catch throws handler rethrows the in-flight error through that channel.

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
  ~error() noexcept;
};
enum class errc : unsigned { invalid = 22 };
template <typename T> class error_domain;
template <> class error_domain<errc> {
public:
  static void *domain() noexcept;
  static __SIZE_TYPE__ code(errc) noexcept;
};
} // namespace std

unsigned foo(int x) throws;

// Deduced return type is `unsigned`; since std::error (16 bytes) is wider
// than the payload, the ABI return type is { { ptr, i64 }, i8 }.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z4testDri(
// OGCG-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z4testDri(
auto test(int x) throws {
  try {
    return foo(x);
  } catch throws(std::error e) {
    throw throws;
  }
}
