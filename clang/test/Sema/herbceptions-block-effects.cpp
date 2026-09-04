// RUN: %clang_cc1 -std=c++20 -fblocks -fherbceptions \
// RUN:   -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++20 -fblocks -fherbceptions \
// RUN:   -verify -ast-dump -ast-dump-filter=plain_nested_in_active %s | \
// RUN:   FileCheck %s --check-prefix=AST

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

int callee(int) throws;

// A block's explicit effect is authoritative even when there is no enclosing
// FunctionDecl, or when its enclosing function has the opposite effect.
auto global_active = ^int(int value) throws {
  if (value < 0)
    throw throws std::block_error{-value};
  return value + 1;
};

void active_nested_in_plain() {
  auto active = ^int(int value) throws {
    if (value < 0)
      throw throws std::block_error{-value};
    return callee(value);
  };
  (void)active;
}

int active_nested_in_active(int value) throws {
  auto active = ^int(int inner) throws {
    if (inner < 0)
      throw throws std::block_error{-inner};
    return callee(inner); // expected-error {{call to 'throws' function in a non-'throws' function}}
  };
  return active(value);
}

int plain_nested_in_active(int value) throws {
  auto plain = ^int(int inner) {
    // This call must remain a plain CallExpr. Inheriting the outer function's
    // effect would manufacture a CXXTryExpr whose block has no failure slot.
    return callee(inner);
  };
  return value + (plain != nullptr);
}

// AST-LABEL: FunctionDecl {{.*}} plain_nested_in_active
// AST: BlockExpr
// AST-NOT: CXXTryExpr
// AST: CallExpr

int rejected_plain_nested_in_active(int value) throws {
  auto plain = ^int(int inner) {
    if (inner < 0)
      throw throws std::block_error{-inner}; // expected-error {{'throw throws' is only allowed inside a function declared 'throws' or 'return_failure{...}'}}
    return inner;
  };
  return value + (plain != nullptr);
}

void throws_false_is_ordinary() {
  auto ordinary = ^int(int value) throws(false) {
    if (value < 0)
      throw throws std::block_error{-value}; // expected-error {{'throw throws' is only allowed inside a function declared 'throws' or 'return_failure{...}'}}
    return value;
  };
  (void)ordinary;
}
