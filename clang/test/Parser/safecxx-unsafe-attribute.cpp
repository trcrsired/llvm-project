// RUN: %clang -fsafecxx -fsyntax-only -Werror -x c++ %s
// RUN: %clang -fsafecxx-unsafe -fsyntax-only -Werror -x c++ %s
// RUN: %clang -fsyntax-only -Werror -x c++ %s

// The fence is an unscoped C++11 attribute. It is recognized by our compiler
// and must not produce an unknown-attribute warning in any mode. Here we only
// establish that the attribute parses cleanly; enforcement of the call
// restriction under -fsafecxx is covered by safecxx-call-restriction.cpp.

[[unsafe]] static void unsafe_to_call() noexcept {}

static void safe_caller() noexcept {
  [[unsafe]] {
    unsafe_to_call();
    int *p = nullptr;
    (void)p;
  }
}
