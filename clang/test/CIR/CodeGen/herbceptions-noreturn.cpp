// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm %s -o - | FileCheck %s

// A function declared with `throws` return type must not be marked noreturn
// in the LLVM IR, even if it never returns normally (e.g. always throws).
// The `[[noreturn]]` attribute is still used by the frontend for diagnostics,
// but the IR must see a normal return so the error value can propagate.

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
  ~error() noexcept;
};
enum class errc : unsigned { io_error = 5 };
template <typename T> class error_domain;
template <> class error_domain<errc> {
public:
  static void *domain() noexcept;
  static __SIZE_TYPE__ code(errc) noexcept;
};
} // namespace std

// This function always throws, so the frontend may consider it [[noreturn]]
// for diagnostic purposes. However, from the IR perspective it returns an
// error value through the herbception mechanism, so it must NOT be noreturn.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z13always_throwsDrv()
// CHECK-NOT: noreturn
// CHECK: ret { { ptr, i64 }, i8 }
void always_throws() throws {
  throw throws ::std::errc::io_error;
}

// Verify the function returns normally (not unreachable after call).
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z6callerDrv()
// CHECK: call { { ptr, i64 }, i8 } @_Z13always_throwsDrv()
// CHECK: ret { { ptr, i64 }, i8 }
void caller() throws {
  always_throws();
}
