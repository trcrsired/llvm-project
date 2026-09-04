// RUN: %clang -std=c++20 -O2 -fherbceptions -fno-exceptions %s -o %t
// RUN: %t
// REQUIRES: native

// Exercise both active alternatives when neither source type dominates both
// payload size and alignment. This is a runtime complement to the IR carrier
// checks: catch return_failure must copy the semantic union arm, not merely
// the LLVM type selected to represent the anonymous union's shared storage.

struct ByteAggregate {
  unsigned char bytes[16];
};
using AlignedScalar = long long;

__attribute__((noinline)) ByteAggregate
aggregate_success(bool fail) return_failure{AlignedScalar} {
  if (fail)
    return_failure 0x112233445566778LL;
  return ByteAggregate{{1, 2, 3, 4, 5, 6, 7, 8,
                        9, 10, 11, 12, 13, 14, 15, 16}};
}

__attribute__((noinline)) AlignedScalar
aggregate_error(bool fail) return_failure{ByteAggregate} {
  if (fail)
    return_failure ByteAggregate{{16, 15, 14, 13, 12, 11, 10, 9,
                                   8, 7, 6, 5, 4, 3, 2, 1}};
  return 0x223344556677889LL;
}

static bool equals(const unsigned char *value, const unsigned char *expected) {
  for (unsigned index = 0; index != 16; ++index)
    if (value[index] != expected[index])
      return false;
  return true;
}

volatile bool fail_flag;

struct ReferenceValue {
  int member;
};

ReferenceValue reference_global{41};
int lvalue_calls;
int xvalue_calls;

ReferenceValue &lvalue_reference(bool fail) return_failure{int} {
  ++lvalue_calls;
  if (fail)
    return_failure 17;
  return reference_global;
}

ReferenceValue &&xvalue_reference(bool fail) return_failure{int} {
  ++xvalue_calls;
  if (fail)
    return_failure 19;
  return static_cast<ReferenceValue &&>(reference_global);
}

int main() {
  const unsigned char ascending[16] = {1, 2, 3, 4, 5, 6, 7, 8,
                                       9, 10, 11, 12, 13, 14, 15, 16};
  const unsigned char descending[16] = {16, 15, 14, 13, 12, 11, 10, 9,
                                        8,  7,  6,  5,  4,  3,  2,  1};

  fail_flag = false;
  auto success = catch return_failure(aggregate_success(fail_flag));
  if (success.failed || !equals(success.value.bytes, ascending))
    return 1;
  auto scalar = catch return_failure(aggregate_error(fail_flag));
  if (scalar.failed || scalar.value != 0x223344556677889LL)
    return 2;

  fail_flag = true;
  auto scalar_error = catch return_failure(aggregate_success(fail_flag));
  if (!scalar_error.failed ||
      scalar_error.error != 0x112233445566778LL)
    return 3;
  auto aggregate = catch return_failure(aggregate_error(fail_flag));
  if (!aggregate.failed || !equals(aggregate.error.bytes, descending))
    return 4;

  // A synthetic reference field stores only the referred-to address. Member
  // access follows the ordinary C++ reference-member rule: even an rvalue-
  // reference member is an lvalue expression, while unparenthesized decltype
  // reports its declared T&& type. Recovering an xvalue therefore remains an
  // explicit operation and must not materialize or decay the referred object.
  auto lvalue_success =
      catch return_failure(lvalue_reference(false));
  if (lvalue_success.failed || &lvalue_success.value != &reference_global ||
      lvalue_calls != 1)
    return 5;
  lvalue_success.value.member = 42;

  auto xvalue_success =
      catch return_failure(xvalue_reference(false));
  static_assert(__is_same(decltype(xvalue_success.value), ReferenceValue &&));
  static_assert(__is_same(decltype((xvalue_success.value)), ReferenceValue &));
  ReferenceValue &&xvalue =
      static_cast<ReferenceValue &&>(xvalue_success.value);
  if (xvalue_success.failed || &xvalue != &reference_global ||
      xvalue.member != 42 || xvalue_calls != 1)
    return 6;

  auto lvalue_error = catch return_failure(lvalue_reference(true));
  if (!lvalue_error.failed || lvalue_error.error != 17 || lvalue_calls != 2)
    return 7;
  auto xvalue_error = catch return_failure(xvalue_reference(true));
  if (!xvalue_error.failed || xvalue_error.error != 19 || xvalue_calls != 2)
    return 8;

  return 0;
}
