// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm -x c++ - -o - < %s | FileCheck %s --check-prefix=NORET --allow-unused-prefixes

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

// An explicit [[noreturn]] declaration on a throws function must likewise not
// produce the IR noreturn attribute: the function physically returns the
// {payload, i1} pair, and marking it noreturn lets LLVM treat the post-call
// error check as unreachable and silently drop the throw.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z8die_declDrv()
// CHECK-NOT: unreachable
// CHECK: ret { { ptr, i64 }, i8 }
[[noreturn]] void die_decl() throws {
  throw throws ::std::errc::io_error;
}

// Same for the GNU spelling, which also marks the function type noreturn.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z7die_gnuDrv()
// CHECK-NOT: unreachable
// CHECK: ret { { ptr, i64 }, i8 }
__attribute__((noreturn)) void die_gnu() throws {
  throw throws ::std::errc::io_error;
}

// Call sites to noreturn throws functions must not be marked noreturn either:
// the discriminant must be checked after the call returns.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z9call_declDrv()
// CHECK: call { { ptr, i64 }, i8 } @_Z8die_declDrv()
// CHECK-NOT: unreachable
// CHECK: ret { { ptr, i64 }, i8 }
void call_decl() throws {
  die_decl();
}

// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z8call_gnuDrv()
// CHECK: call { { ptr, i64 }, i8 } @_Z7die_gnuDrv()
// CHECK-NOT: unreachable
// CHECK: ret { { ptr, i64 }, i8 }
void call_gnu() throws {
  die_gnu();
}

// The noreturn attribute must appear nowhere in the output: not on the
// function definitions and not in any attribute group.
// NORET-NOT: noreturn
