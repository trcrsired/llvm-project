// RUN: %clang_cc1 -std=c++20 -fherbceptions -fblocks -emit-llvm -verify %s -o /dev/null

namespace std {
struct error_domain_singleton {};
struct error {
  error_domain_singleton const *domain;
  __SIZE_TYPE__ code;
};
template <class T> class error_domain;
inline error_domain_singleton int_domain;
template <> class error_domain<int> {
public:
  static error_domain_singleton const *domain() noexcept {
    return &int_domain;
  }
  static __SIZE_TYPE__ code(int value) noexcept { return value; }
};
} // namespace std

int fail_int() return_failure{int} { return_failure 7; }

void caught_by_same_callable() {
  try {
    (void)fail_int();
  } catch throws(std::error) {
  }
}

void closure_boundaries() {
  try {
    auto lambda = [] {
      (void)fail_int(); // expected-error {{call to 'throws' function}}
    };
    auto block = ^{
      (void)fail_int(); // expected-error {{call to 'throws' function}}
    };
    // Force both closure bodies to be emitted; merely naming a lambda need
    // not instantiate its out-of-line call operator during this test.
    lambda();
    block();
  } catch throws(std::error) {
  }

  auto locally_caught_lambda = [] {
    try {
      (void)fail_int();
    } catch throws(std::error) {
    }
  };
  auto locally_caught_block = ^{
    try {
      (void)fail_int();
    } catch throws(std::error) {
    }
  };
  locally_caught_lambda();
  locally_caught_block();
}

struct missing_domain_error {
  int value;
};

int fail_without_domain() return_failure{missing_domain_error} {
  return_failure missing_domain_error{8};
}

void missing_domain_is_not_reinterpreted() {
  try {
    (void)fail_without_domain(); // expected-error {{no std::error_domain specialization}}
  } catch throws(std::error) {
  }
}
