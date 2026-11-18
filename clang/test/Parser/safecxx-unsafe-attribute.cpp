// RUN: %clang -fsafecxx -fsyntax-only -Werror -x c++ %s
// RUN: %clang -fsafecxx-unsafe -fsyntax-only -Werror -x c++ %s
// RUN: %clang -fsyntax-only -Werror -x c++ %s

// The fence is an unscoped C++11 attribute. It is recognized by our compiler
// and must not produce an unknown-attribute warning in any mode. Giving it a
// meaning (restricting pointers, calls, etc.) is a later step and is gated by
// -fsafecxx; here we only establish that the attribute parses cleanly.

[[unsafe]] void unsafe_to_call() noexcept {}

void safe_caller() noexcept {
  // A call restriction is not enforced yet, but the spelling must parse.
  unsafe_to_call();

  [[unsafe]] {
    int *p = nullptr;
    (void)p;
  }
}
