// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fblocks \
// RUN:   -fherbceptions -emit-llvm -o - %s | FileCheck %s

namespace std {
struct error_domain_singleton {};
struct error {
  void *domain;
  __SIZE_TYPE__ code;
};
struct block_error {
  int value;
};
template <class T> class error_domain;
template <> class error_domain<block_error> {
public:
  static error_domain_singleton const *domain() noexcept;
  static __SIZE_TYPE__ code(block_error error) noexcept {
    return static_cast<__SIZE_TYPE__>(error.value);
  }
};
}

using active_block = int (^)(int) throws;
using ordinary_block = int (^)(int) throws(false);

active_block global_active = ^int(int value) throws {
  if (value < 0)
    throw throws std::block_error{-value};
  return value + 1;
};

ordinary_block global_ordinary = ^int(int value) throws(false) {
  return value + 2;
};

// The descriptor's active invoke entry and an indirect call through a block
// pointer must agree on the shaped deterministic return type.
// CHECK-LABEL: define internal { { ptr, i64 }, i1 } @global_active_block_invoke(
// CHECK: ret { { ptr, i64 }, i1 }
// CHECK-LABEL: define internal noundef i32 @global_ordinary_block_invoke(
// CHECK: ret i32

int invoke(active_block operation, int value) throws {
  return operation(value);
}

// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z6invokeU13block_pointerFiiDu9_throwsEi(
// CHECK: call { { ptr, i64 }, i1 } %{{.*}}(
// CHECK: extractvalue { { ptr, i64 }, i1 } %{{.*}}, 1

int plain_nested_in_active(int value) throws {
  auto plain = ^int(int inner) { return inner + 3; };
  return plain(value);
}

// A plain nested block remains an ordinary i32 invoke entry even though its
// enclosing function owns a deterministic result channel.
// CHECK-LABEL: define internal noundef i32 @___Z22plain_nested_in_activei_block_invoke(
// CHECK: ret i32
