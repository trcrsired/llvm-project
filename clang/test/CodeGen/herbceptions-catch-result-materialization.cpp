// RUN: %clang -std=c++20 -O0 -fherbceptions -fno-exceptions %s -o %t.o0
// RUN: %t.o0
// RUN: %clang -std=c++20 -O2 -fherbceptions -fno-exceptions %s -o %t.o2
// RUN: %t.o2
// REQUIRES: native

int argument_live;
int argument_destructions;
int value_live;
int value_constructions;
int value_destructions;

struct argument {
  argument() { ++argument_live; }
  argument(argument const &) = delete;
  ~argument() {
    --argument_live;
    ++argument_destructions;
  }
};

struct value {
  int payload;
  value *self;

  explicit value(int input) : payload(input), self(this) {
    ++value_live;
    ++value_constructions;
  }
  value(value const &) = delete;
  value(value &&) = delete;
  ~value() {
    --value_live;
    ++value_destructions;
  }
};

__attribute__((noinline)) value make_plain() return_failure{int} {
  return value(41);
}

__attribute__((noinline)) value make_with_argument(argument const &)
    return_failure{int} {
  return value(43);
}

template <class Result>
__attribute__((noinline)) int inspect(Result const &result,
                                      int expected_argument_live,
                                      int expected_payload) {
  // The failure discriminator is the active-member precondition for every
  // access to value; a failed result has no live success union member.
  if (result.failed)
    return 1;
  // Distinct bits keep the first remote execution diagnostic: the caller can
  // separate a lost discriminator from the destination slot, inner-argument
  // lifetime, and success-object lifetime without relying on debugger state.
  return (result.value.payload != expected_payload ? 2 : 0) |
         (argument_live != expected_argument_live ? 4 : 0) |
         (value_live != 1 ? 8 : 0) |
         (result.value.self != __builtin_addressof(result.value) ? 16 : 0);
}

int main() {
  {
    // A temporary catch result with no callee argument isolates the outer
    // MaterializeTemporaryExpr and reference-binding path.
    int bits = inspect(catch return_failure(make_plain()), 0, 41);
    if (bits)
      return 0x20 | bits;
  }
  if (value_live != 0 || value_constructions != 1 ||
      value_destructions != 1)
    return 0x3f;

  {
    // A stable lvalue argument distinguishes callee signature lowering from
    // full-expression ownership of a temporary argument.
    argument stable;
    int bits = inspect(catch return_failure(make_with_argument(stable)), 1, 43);
    if (bits)
      return 0x40 | bits;
  }
  if (argument_live != 0 || argument_destructions != 1 || value_live != 0 ||
      value_constructions != 2 || value_destructions != 2)
    return 0x5f;

  {
    // Both the inner argument and outer result are full-expression
    // temporaries. The result must be inspected while both are alive and must
    // be destroyed first because its cleanup is registered last.
    int bits = inspect(
        catch return_failure(make_with_argument(argument{})), 1, 43);
    if (bits)
      return 0x60 | bits;
  }
  if (argument_live != 0 || argument_destructions != 2 || value_live != 0 ||
      value_constructions != 3 || value_destructions != 3)
    return 0x7f;

  {
    // Naming the catch result removes only its outer materialization as a call
    // argument; the inner temporary must already be gone after initialization.
    auto named = catch return_failure(make_with_argument(argument{}));
    int bits = inspect(named, 0, 43);
    if (bits)
      return 0x80 | bits;
  }
  if (argument_live != 0 || argument_destructions != 3 || value_live != 0 ||
      value_constructions != 4 || value_destructions != 4)
    return 0x9f;

  return 0;
}
