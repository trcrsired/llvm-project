// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm %s -o - | FileCheck %s

// A constructor that throws a herbception inside a `try { } catch throws(...)`
// block must route the error to the catch handler.

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

// Struct that throws on construction
struct bad_file {
  int fd;
  bad_file() throws {
    throw throws ::std::errc::io_error;
  }
  ~bad_file() noexcept {}
};

// Struct that succeeds on construction
struct good_file {
  int fd;
  good_file() throws : fd(1) {}
  ~good_file() noexcept {}
};

// When the first constructor throws, the catch handler must be reachable:
// the call's discriminant selects between the error path (store the error
// into the handler slot and jump to the handler) and the success path
// (construct good_file).
// CHECK-LABEL: define dso_local noundef i32 @_Z8test_bugv()
// CHECK: {{call|invoke}} { { ptr, i64 }, i8 } @_ZN8bad_fileC{{[12]}}EDrv
// CHECK: br i1
// CHECK: store %"struct.std::error"
// CHECK: ret i32
int test_bug() try {
  bad_file f1;      // Throws
  good_file f2;     // Should not reach
  return 0;
} catch throws(std::error e) {
  return 1;
}
