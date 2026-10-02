// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm %s -o - | FileCheck %s

// Bare `throw throws` rethrow moves the in-flight error out of the handler:
// the handler's own copy is left untouched, and the enclosing function
// returns the error with the discriminant set.

namespace std {

struct error_domain_singleton {};

struct error {
  void *d;
  __SIZE_TYPE__ c;
  ~error();
};

enum class errc {
  success = 0,
  io_error = 5,
  network_down = 6,
};

inline bool operator==(const error &e, errc v) {
  return e.c == (__SIZE_TYPE__)v;
}

template <class T> class error_domain;

template <> class error_domain<errc> {
public:
  static const error_domain_singleton *domain() noexcept;
  static __SIZE_TYPE__ code(errc e) noexcept { return (__SIZE_TYPE__)e; }
};

} // namespace std

void foo(int) throws;

// The bare rethrow loads the handler's error slot and returns it with the
// discriminant set.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z4bareDri(i32 noundef %0)
// CHECK: {{call|invoke}} { { ptr, i64 }, i8 } @_Z3fooDri
// CHECK: store i8 1
// CHECK: ret { { ptr, i64 }, i8 }
void bare(int x) throws {
  try {
    foo(x);
  } catch throws(std::error e) {
    (void)e;
    throw throws;
  }
}

// `throw throws e` constructs a fresh error from the handler's copy and
// returns it.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z4copyDri(i32 noundef %0)
// CHECK: {{call|invoke}} { { ptr, i64 }, i8 } @_Z3fooDri
// CHECK: ret { { ptr, i64 }, i8 }
void copy(int x) throws {
  try {
    foo(x);
  } catch throws(std::error e) {
    throw throws e;
  }
}
