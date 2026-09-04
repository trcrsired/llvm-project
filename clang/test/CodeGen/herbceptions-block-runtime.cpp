// RUN: %clang -std=c++20 -O2 -fblocks -fherbceptions -fno-exceptions %s -o %t
// RUN: %t
// REQUIRES: native

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
  static error_domain_singleton const *domain() noexcept {
    static error_domain_singleton singleton;
    return &singleton;
  }
  static __SIZE_TYPE__ code(block_error error) noexcept {
    return static_cast<__SIZE_TYPE__>(error.value);
  }
};
}

// No copy operation is used by this test, but Linux has no system Blocks
// runtime. These ABI class objects are sufficient for invoking global and
// stack literals while they remain in their defining scope.
extern "C" void *_NSConcreteGlobalBlock[32];
extern "C" void *_NSConcreteStackBlock[32];
void *_NSConcreteGlobalBlock[32];
void *_NSConcreteStackBlock[32];

using active_block = int (^)(int) throws;

int calls;
active_block global_active = ^int(int value) throws {
  ++calls;
  if (value < 0)
    throw throws std::block_error{-value};
  return value + 10;
};

int leaf_calls;
__attribute__((noinline)) int leaf(int value) throws {
  ++leaf_calls;
  if (value < 0)
    throw throws std::block_error{100 - value};
  return value + 100;
}

// This literal checks the direct-call side of routing: its invoke function
// must auto-propagate leaf's failure through the block's own result channel.
active_block forwarding = ^int(int value) throws {
  return leaf(value);
};

__attribute__((noinline)) int observe(active_block operation, int value) {
  try {
    return operation(value);
  } catch throws(std::error error) {
    return -static_cast<int>(error.code);
  }
}

int main() {
  if (observe(global_active, 2) != 12 || calls != 1)
    return 1;
  if (observe(global_active, -7) != -7 || calls != 2)
    return 2;

  if (observe(forwarding, 5) != 105 || leaf_calls != 1)
    return 3;
  if (observe(forwarding, -6) != -106 || leaf_calls != 2)
    return 4;

  int bias = 20;
  active_block local_active = ^int(int value) throws {
    ++calls;
    if (value < 0)
      throw throws std::block_error{bias - value};
    return bias + value;
  };
  if (observe(local_active, 3) != 23 || calls != 3)
    return 5;
  if (observe(local_active, -4) != -24 || calls != 4)
    return 6;

  auto ordinary = ^int(int value) throws(false) { return value + 30; };
  if (ordinary(5) != 35)
    return 7;
  return 0;
}
