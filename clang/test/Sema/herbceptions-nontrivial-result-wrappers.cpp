// RUN: %clang_cc1 -std=c++20 -fherbceptions \
// RUN:   -fsyntax-only -verify %s
// expected-no-diagnostics

struct tracked {
  int value;
  explicit tracked(int input) : value(input) {}
  tracked(const tracked &) = delete;
  tracked(tracked &&) = delete;
  ~tracked();
};

tracked make_tracked(bool fail) return_failure{int} {
  if (fail)
    return_failure 17;
  return tracked(42);
}

tracked propagate_tracked(bool fail) return_failure{int} {
  return try(make_tracked(fail));
}

int inspect_tracked(bool fail) {
  auto result = catch return_failure(make_tracked(fail));
  return result.failed ? result.error : result.value.value;
}
