// RUN: %clang -std=c++20 -O0 -fherbceptions -fno-exceptions %s -o %t.o0
// RUN: %t.o0
// RUN: %clang -std=c++20 -O2 -fherbceptions -fno-exceptions %s -o %t.o2
// RUN: %t.o2
// REQUIRES: native

int value_live;
int value_constructions;
int value_destructions;
int argument_live;
int argument_destructions;
int calls;
int void_calls;
int alternate_destructions;
int destruction_sequence;
int value_destruction_order;
int argument_destruction_order;
bool record_destruction_order;

struct reference_value {
  int value;
};

reference_value reference_object{61};
int lvalue_reference_calls;
int xvalue_reference_calls;

struct tracked_value {
  int value;

  explicit tracked_value(int input) : value(input) {
    ++value_live;
    ++value_constructions;
  }
  tracked_value(tracked_value const &) = delete;
  tracked_value(tracked_value &&) = delete;
  ~tracked_value() {
    if (record_destruction_order) {
      value_destruction_order = ++destruction_sequence;
    }
    --value_live;
    ++value_destructions;
  }
};

struct argument_temporary {
  argument_temporary() { ++argument_live; }
  argument_temporary(argument_temporary const &) = delete;
  ~argument_temporary() {
    if (record_destruction_order) {
      argument_destruction_order = ++destruction_sequence;
    }
    --argument_live;
    ++argument_destructions;
  }
};

struct alternate_value {
  int value;
  explicit alternate_value(int input) : value(input) {}
  alternate_value(alternate_value const &) = delete;
  alternate_value(alternate_value &&) = delete;
  ~alternate_value() { ++alternate_destructions; }
};

__attribute__((noinline)) tracked_value make_value(bool fail) return_failure{
    int} {
  ++calls;
  if (fail) {
    return_failure 19;
  }
  return tracked_value(41);
}

__attribute__((noinline)) tracked_value make_value_with_argument(
    argument_temporary const &, bool fail) return_failure{int} {
  ++calls;
  if (fail) {
    return_failure 29;
  }
  return tracked_value(43);
}

__attribute__((noinline)) alternate_value make_alternate(bool fail)
    return_failure{int} {
  if (fail) {
    return_failure 31;
  }
  return alternate_value(47);
}

__attribute__((noinline)) void make_void(bool fail) return_failure{int} {
  ++void_calls;
  if (fail) {
    return_failure 37;
  }
}

__attribute__((noinline)) reference_value &
make_lvalue_reference(bool fail) return_failure{int} {
  ++lvalue_reference_calls;
  if (fail)
    return_failure 67;
  return reference_object;
}

__attribute__((noinline)) reference_value &&
make_xvalue_reference(bool fail) return_failure{int} {
  ++xvalue_reference_calls;
  if (fail)
    return_failure 71;
  return static_cast<reference_value &&>(reference_object);
}

struct incomplete_value;
incomplete_value &make_incomplete_reference() return_failure { int };

using incomplete_reference_result =
    decltype(catch return_failure(make_incomplete_reference()));
extern incomplete_reference_result *incomplete_result_pointer;
static_assert(__is_same(decltype(incomplete_result_pointer->value),
                        incomplete_value &));
static_assert(__is_same(decltype((incomplete_result_pointer->value)),
                        incomplete_value &));

template <class Result>
__attribute__((noinline)) int inspect_catch_result(Result const &result) {
  // A temporary call argument belongs to the full expression containing this
  // consumer, not merely to the inner catch-return wrapper.
  return argument_live == 1 && !result.failed && result.value.value == 43;
}

__attribute__((noinline)) int inspect_value(tracked_value const &value) {
  return argument_live == 1 && value.value == 43;
}

__attribute__((noinline)) int inspect_try_value(bool fail) return_failure{int} {
  int observed =
      inspect_value(try(make_value_with_argument(argument_temporary{}, fail)));
  if (!observed) {
    return_failure 97;
  }
  // The argument and success value remain alive inside inspect_value, then
  // both end at the enclosing initialization full-expression.
  if (argument_live != 0 || value_live != 0) {
    return_failure 97;
  }
  return 53;
}

using nontrivial_result = decltype(catch return_failure(make_value(false)));
static_assert(!__is_constructible(nontrivial_result));
static_assert(!__is_constructible(nontrivial_result,
                                  nontrivial_result const &));
static_assert(!__is_constructible(nontrivial_result, nontrivial_result &&));
static_assert(!__is_assignable(nontrivial_result &, nontrivial_result const &));
static_assert(!__is_assignable(nontrivial_result &, nontrivial_result &&));

__attribute__((noinline)) int make_trivial(bool fail) return_failure{int} {
  if (fail) {
    return_failure 41;
  }
  return 59;
}
using trivial_result = decltype(catch return_failure(make_trivial(false)));
static_assert(__is_aggregate(trivial_result));
static_assert(__is_trivially_copyable(trivial_result));

int main() {
  {
    auto success = catch return_failure(make_value(false));
    if (success.failed || success.value.value != 41 || value_live != 1 ||
        calls != 1) {
      return 1;
    }
  }
  if (value_live != 0 || value_constructions != 1 || value_destructions != 1) {
    return 2;
  }

  {
    auto failure = catch return_failure(make_value(true));
    if (!failure.failed || failure.error != 19 || value_live != 0 ||
        calls != 2) {
      return 3;
    }
  }
  if (value_constructions != 1 || value_destructions != 1) {
    return 4;
  }

  catch return_failure(make_value(false));
  if (value_live != 0 || value_constructions != 2 || value_destructions != 2 ||
      calls != 3) {
    return 5;
  }

  destruction_sequence = 0;
  value_destruction_order = 0;
  argument_destruction_order = 0;
  record_destruction_order = true;
  int observed = inspect_catch_result(catch return_failure(
      make_value_with_argument(argument_temporary{}, false)));
  record_destruction_order = false;
  if (!observed || argument_live != 0 || argument_destructions != 1 ||
      value_constructions != 3 || value_destructions != 3 || calls != 4 ||
      value_destruction_order != 1 || argument_destruction_order != 2) {
    return 6;
  }

  auto ignored_success = catch return_failure(inspect_try_value(false));
  if (ignored_success.failed || ignored_success.value != 53 ||
      argument_live != 0 || argument_destructions != 2 ||
      value_constructions != 4 || value_destructions != 4 || calls != 5) {
    return 7;
  }

  auto ignored_failure = catch return_failure(inspect_try_value(true));
  if (!ignored_failure.failed || ignored_failure.error != 29 ||
      argument_live != 0 || argument_destructions != 3 ||
      value_constructions != 4 || value_destructions != 4 || calls != 6) {
    return 8;
  }

  {
    auto first = catch return_failure(make_value(false));
    auto second = catch return_failure(make_alternate(false));
    if (first.failed || first.value.value != 41 || second.failed ||
        second.value.value != 47) {
      return 9;
    }
  }
  if (value_destructions != 5 || alternate_destructions != 1) {
    return 10;
  }

  {
    auto success = catch return_failure(make_void(false));
    if (success.failed || void_calls != 1) {
      return 11;
    }
  }
  {
    auto failure = catch return_failure(make_void(true));
    if (!failure.failed || failure.error != 37 || void_calls != 2) {
      return 12;
    }
  }

  {
    auto success = catch return_failure(make_lvalue_reference(false));
    static_assert(__is_same(decltype(success.value), reference_value &));
    static_assert(__is_same(decltype((success.value)), reference_value &));
    if (success.failed || &success.value != &reference_object ||
        lvalue_reference_calls != 1)
      return 13;
    success.value.value = 73;
  }
  {
    auto success = catch return_failure(make_xvalue_reference(false));
    static_assert(__is_same(decltype(success.value), reference_value &&));
    static_assert(__is_same(decltype((success.value)), reference_value &));
    if (success.failed || &success.value != &reference_object ||
        success.value.value != 73 || xvalue_reference_calls != 1)
      return 14;
  }
  {
    // Failure initializes only the error arm. In particular, lowering must
    // not load through the indeterminate reference representation in `value`.
    auto failure = catch return_failure(make_lvalue_reference(true));
    if (!failure.failed || failure.error != 67 || lvalue_reference_calls != 2 ||
        reference_object.value != 73)
      return 15;
  }
  {
    auto failure = catch return_failure(make_xvalue_reference(true));
    if (!failure.failed || failure.error != 71 || xvalue_reference_calls != 2 ||
        reference_object.value != 73)
      return 16;
  }
  return 0;
}
