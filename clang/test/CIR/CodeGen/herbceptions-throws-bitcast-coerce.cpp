// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions -fclangir -emit-llvm %s -o - | FileCheck %s

// A void throws function calling a throws function that returns a struct
// of the same machine layout as the enclosing function's return slot
// cannot bitcast between the two aggregate types under opaque pointers --
// the success value has to be coerced through memory instead.

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
  ~error() noexcept;
};
template <class T> class error_domain;
template <> class error_domain<int> {
public:
  static void *domain() noexcept;
  static __SIZE_TYPE__ code(int) noexcept;
};
} // namespace std

// Match the size of std::error on each target:
//   x86_64:  void* (8) + size_t (8) = 16 bytes;  inner uses {i64, i64}
//   i686:    void* (4) + size_t (4) =  8 bytes;  inner uses {i32, i32}
namespace ns {
#ifdef __SIZEOF_POINTER__
#if __SIZEOF_POINTER__ == 8
struct status { long long p, n; };
#else
struct status { int p, n; };
#endif
#else
struct status { long long p, n; };
#endif
}

// CHECK-LABEL: define dso_local { %"struct.ns::status", i8 } @_Z5innerDrv()
// CHECK: ret { %"struct.ns::status", i8 }
__attribute__((noinline)) ns::status inner() throws {
  ns::status s;
  asm volatile ("" : "=r"(s.p), "=r"(s.n));
  return s;
}

// The shaped result is stored through memory and its discriminant drives
// the propagate/continue branch.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i8 } @_Z5outerDrv()
// CHECK: call { %"struct.ns::status", i8 } @_Z5innerDrv()
// CHECK: br i1
// CHECK: ret { { ptr, i64 }, i8 }
__attribute__((noinline)) void outer() throws {
  for (;;) {
    ns::status ret = inner();
    if (ret.p == 0) return;
  }
}
