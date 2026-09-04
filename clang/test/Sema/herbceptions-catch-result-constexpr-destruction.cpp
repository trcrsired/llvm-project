// RUN: %clang_cc1 -std=c++20 -fherbceptions -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++23 -fherbceptions -fsyntax-only -verify %s

struct constexpr_tracked {
  int *destructions;
  constexpr_tracked *self;

  constexpr explicit constexpr_tracked(int *count)
      : destructions(count), self(this) {}
  constexpr_tracked(const constexpr_tracked &) = delete;
  constexpr_tracked(constexpr_tracked &&) = delete;
  constexpr ~constexpr_tracked() { ++*destructions; }
};

constexpr constexpr_tracked
make_constexpr_tracked(int *destructions, bool fail) return_failure{int} {
  if (fail)
    return_failure 17;
  return constexpr_tracked(destructions);
}

constexpr int success_destructions() {
  int destructions = 0;
  {
    auto result =
        catch return_failure(make_constexpr_tracked(&destructions, false));
    if (result.failed || result.value.destructions != &destructions ||
        result.value.self != &result.value || destructions != 0)
      return -1;
  }
  return destructions;
}

constexpr int discarded_success_destructions() {
  int destructions = 0;
  (void)(catch return_failure(
      make_constexpr_tracked(&destructions, false)));
  return destructions;
}

constexpr constexpr_tracked
forward_constexpr_tracked(int *destructions) return_failure{int} {
  return try(make_constexpr_tracked(destructions, false));
}

constexpr int nested_try_destructions() {
  int destructions = 0;
  {
    auto result = catch return_failure(
        forward_constexpr_tracked(&destructions));
    if (result.failed || result.value.self != &result.value ||
        destructions != 0)
      return -1;
  }
  return destructions;
}

constexpr int failure_destructions() {
  int destructions = 0;
  {
    auto result =
        catch return_failure(make_constexpr_tracked(&destructions, true));
    if (!result.failed || result.error != 17)
      return -1;
  }
  return destructions;
}

struct constexpr_error_tracked {
  int *destructions;
  constexpr_error_tracked *self;

  constexpr explicit constexpr_error_tracked(int *count)
      : destructions(count), self(this) {
    // Exercise member lookup through `this` while the error union arm is
    // under construction; selecting that arm only after the call is too late.
    self = this;
  }
  constexpr_error_tracked(const constexpr_error_tracked &) = delete;
  constexpr_error_tracked(constexpr_error_tracked &&) = delete;
  constexpr ~constexpr_error_tracked() { ++*destructions; }
};

constexpr int make_constexpr_error(int *destructions)
    return_failure{constexpr_error_tracked} {
  return_failure constexpr_error_tracked(destructions);
}

constexpr int failure_payload_destructions() {
  int destructions = 0;
  {
    auto result =
        catch return_failure(make_constexpr_error(&destructions));
    if (!result.failed || result.error.self != &result.error ||
        destructions != 0)
      return -1;
  }
  return destructions;
}

constexpr int forward_constexpr_error(int *destructions)
    return_failure{constexpr_error_tracked} {
  return try(make_constexpr_error(destructions));
}

constexpr int nested_failure_payload_destructions() {
  int destructions = 0;
  {
    auto result =
        catch return_failure(forward_constexpr_error(&destructions));
    if (!result.failed || result.error.self != &result.error ||
        destructions != 0)
      return -1;
  }
  return destructions;
}

static_assert(success_destructions() == 1,
              "constant evaluation must destroy the active success arm");
static_assert(failure_destructions() == 0,
              "constant evaluation must not destroy an unconstructed value");
static_assert(discarded_success_destructions() == 1,
              "a discarded catch result must destroy its value exactly once");
static_assert(nested_try_destructions() == 1,
              "try must preserve direct construction through propagation");
static_assert(failure_payload_destructions() == 1,
              "the active error payload must be constructed in place");
static_assert(nested_failure_payload_destructions() == 1,
              "try must preserve the final error payload destination");

#if __cplusplus >= 202302L
struct runtime_only_destruction {
  ~runtime_only_destruction();
};

constexpr runtime_only_destruction
fail_before_constructing_runtime_value() return_failure{int} {
  return_failure 23;
}

constexpr bool cxx23_inactive_non_constexpr_alternative() {
  auto result = catch return_failure(fail_before_constructing_runtime_value());
  return result.failed && result.error == 23;
}

// Since C++23, a non-selected alternative with a non-constexpr destructor
// does not prevent constant evaluation of the failure arm.
static_assert(cxx23_inactive_non_constexpr_alternative());
#endif

// expected-no-diagnostics
