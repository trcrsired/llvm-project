// RUN: %clang_cc1 -std=c++20 -fherbceptions -fsyntax-only -verify %s

struct Smaller {
  unsigned char value;
};

struct Larger {
  unsigned long long value[2];
};

struct SameSizeA {
  unsigned value;
};

struct SameSizeB {
  unsigned value;
};

static_assert(sizeof(Smaller) < sizeof(Larger));
static_assert(sizeof(SameSizeA) == sizeof(SameSizeB));

int rejectLargerAsSmaller(Larger Value) return_failure{Smaller} {
  return_failure Value; // expected-error {{types must match exactly}}
}

int rejectSmallerAsLarger(Smaller Value) return_failure{Larger} {
  return_failure Value; // expected-error {{types must match exactly}}
}

int rejectSameSizeDistinct(SameSizeB Value) return_failure{SameSizeA} {
  return_failure Value; // expected-error {{types must match exactly}}
}

using CanonicalAlias = SameSizeA;

int acceptCanonicalTypedef(CanonicalAlias Value) return_failure{SameSizeA} {
  return_failure Value;
}

int acceptSameTypeLvalue(SameSizeA &Value) return_failure{SameSizeA} {
  return_failure Value;
}

int acceptSameTypePrvalue() return_failure{SameSizeA} {
  return_failure SameSizeA{17};
}

int acceptConstScalar(const int &Value) return_failure{const int} {
  return_failure Value;
}

using MutableFailureFunction = int() return_failure{int};
using ConstFailureFunction = int() return_failure{const int};
using VolatileFailureFunction = int() return_failure{volatile int};
using MutablePointeeFailureFunction = int() return_failure{int *};
using ConstPointeeFailureFunction = int() return_failure{const int *};

static_assert(__is_same(MutableFailureFunction, ConstFailureFunction));
static_assert(__is_same(MutableFailureFunction, VolatileFailureFunction));
static_assert(
    !__is_same(MutablePointeeFailureFunction, ConstPointeeFailureFunction));

using ConstFailureResult =
    __invoke_herbceptions_return_failure_result<ConstFailureFunction>;
static_assert(__is_same(typename ConstFailureResult::error_type, int));

int cvRedeclaration() return_failure{int};
int cvRedeclaration() return_failure{const int};

MutableFailureFunction *MutableFailurePointer = &cvRedeclaration;
ConstFailureFunction *ConstFailurePointer = MutableFailurePointer;

int rejectReferenceErrorType()
    return_failure{SameSizeA &}; // expected-error {{must be an object type}}

struct DeletedCopy {
  int value;
  DeletedCopy() = default;
  DeletedCopy(const DeletedCopy &) = delete; // expected-note {{explicitly marked deleted here}}
  DeletedCopy(DeletedCopy &&) = default;
};
static_assert(__is_trivially_copyable(DeletedCopy));

int rejectDeletedCopy(const DeletedCopy &Value) return_failure{DeletedCopy} {
  return_failure Value; // expected-error {{call to deleted constructor}}
}

struct DeletedMove {
  int value;
  DeletedMove() = default;
  DeletedMove(const DeletedMove &) = default;
  DeletedMove(DeletedMove &&) = delete; // expected-note {{explicitly marked deleted here}}
};
static_assert(__is_trivially_copyable(DeletedMove));

int rejectDeletedMove(DeletedMove &Value) return_failure{DeletedMove} {
  return_failure static_cast<DeletedMove &&>(Value); // expected-error {{call to deleted constructor}}
}

struct InaccessibleCopy {
  int value;
  InaccessibleCopy() = default;
  InaccessibleCopy(InaccessibleCopy &&) = default;

private:
  InaccessibleCopy(const InaccessibleCopy &) = default; // expected-note {{declared private here}}
};
static_assert(__is_trivially_copyable(InaccessibleCopy));

int rejectInaccessibleCopy(const InaccessibleCopy &Value)
    return_failure{InaccessibleCopy} {
  return_failure Value; // expected-error {{calling a private constructor}}
}

struct InaccessibleMove {
  int value;
  InaccessibleMove() = default;
  InaccessibleMove(const InaccessibleMove &) = default;

private:
  InaccessibleMove(InaccessibleMove &&) = default; // expected-note {{declared private here}}
};
static_assert(__is_trivially_copyable(InaccessibleMove));

int rejectInaccessibleMove(InaccessibleMove &Value)
    return_failure{InaccessibleMove} {
  return_failure static_cast<InaccessibleMove &&>(Value); // expected-error {{calling a private constructor}}
}

// Top-level cv does not change the payload object type. It reaches ordinary
// result initialization, where the selected volatile copy is correctly
// rejected as deleted.
struct VolatileSource {
  int value;
  VolatileSource() = default;
  VolatileSource(const VolatileSource &) = default;
  VolatileSource(const volatile VolatileSource &) = delete; // expected-note {{explicitly marked deleted here}}
};
static_assert(__is_trivially_copyable(VolatileSource));

int rejectVolatileSource(volatile VolatileSource &Value)
    return_failure{VolatileSource} {
  return_failure Value; // expected-error {{call to deleted constructor}}
}
