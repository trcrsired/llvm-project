// RUN: %clang -std=c++20 -O2 -fherbceptions -fno-exceptions %s -o %t
// RUN: %t
// REQUIRES: native

int live_objects;
int constructions;
int destructions;

struct tracked {
  int value;

  explicit tracked(int input) : value(input) {
    ++live_objects;
    ++constructions;
  }
  tracked(const tracked &) = delete;
  tracked(tracked &&) = delete;
  ~tracked() {
    --live_objects;
    ++destructions;
  }
};

__attribute__((noinline)) tracked
make_tracked(bool fail) return_failure{int} {
  if (fail)
    return_failure 17;
  return tracked(42);
}

__attribute__((noinline)) int
propagate_tracked(bool fail) return_failure{int} {
  tracked value = try(make_tracked(fail));
  return value.value;
}

int main() {
  {
    auto success = catch return_failure(make_tracked(false));
    if (success.failed || success.value.value != 42 || live_objects != 1)
      return 1;
  }
  if (live_objects != 0 || constructions != 1 || destructions != 1)
    return 2;

  {
    auto failure = catch return_failure(make_tracked(true));
    if (!failure.failed || failure.error != 17 || live_objects != 0)
      return 3;
  }
  // The failure arm never constructed `tracked`, so leaving the synthetic
  // result must not run a destructor for its inactive value member.
  if (constructions != 1 || destructions != 1)
    return 4;

  auto propagated_success = catch return_failure(propagate_tracked(false));
  if (propagated_success.failed || propagated_success.value != 42 ||
      constructions != 2 || destructions != 2 || live_objects != 0)
    return 5;

  auto propagated_failure = catch return_failure(propagate_tracked(true));
  if (!propagated_failure.failed || propagated_failure.error != 17 ||
      constructions != 2 || destructions != 2 || live_objects != 0)
    return 6;
  return 0;
}
