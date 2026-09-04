// clang-format off
// RUN: %clang -std=c++20 -fherbceptions -fno-exceptions %s -o %t.noeh
// RUN: %t.noeh
// RUN: %clang -std=c++20 -fherbceptions -fexceptions -DENABLE_LEGACY_EH %s -o %t.eh
// RUN: %t.eh
// REQUIRES: native
// clang-format on

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
};
} // namespace std

#ifdef ENABLE_LEGACY_EH
extern "C" void *__cxa_error_domain_itanium_exception_ptr() noexcept;
extern "C" unsigned long long __cxa_error_code_itanium_exception_ptr(void *);
#endif

static int Events[64];
static int EventCount;

static void record(int Event) { Events[EventCount++] = Event; }

template <__SIZE_TYPE__ N> static bool matches(const int (&Expected)[N]) {
  if (EventCount != static_cast<int>(N))
    return false;
  for (__SIZE_TYPE__ I = 0; I != N; ++I)
    if (Events[I] != Expected[I])
      return false;
  return true;
}

static void reset() { EventCount = 0; }

struct TrackedBaseMember {
  TrackedBaseMember() noexcept { record(1); }
  ~TrackedBaseMember() { record(-1); }
};

struct TrackedBase {
  TrackedBaseMember member;
  TrackedBase() noexcept = default;
  ~TrackedBase() { record(-101); }
};

struct Tracked {
  int id;
  Tracked(int Id) noexcept : id(Id) { record(Id); }
  ~Tracked() { record(-id); }
};

struct FallibleTail {
  explicit FallibleTail(bool Fail) throws {
    record(5);
    if (Fail)
      throw throws std::error{nullptr, 11};
  }
  ~FallibleTail() { record(-5); }
};

struct Composite : TrackedBase {
  Tracked member;
  Tracked array[2];
  FallibleTail tail;

  explicit Composite(int Mode) throws : TrackedBase(),
                                        member(2),
                                        array{{3}, {4}},
                                        tail(Mode == 1) {
    if (Mode == 2)
      throw throws std::error{nullptr, 12};
    if (Mode == 3)
      return;
    record(6);
  }

  ~Composite() { record(-106); }
};

static bool testMemberFailure() {
  reset();
  try {
    Composite value(1);
    (void)value;
    return false;
  } catch throws(std::error error) {
    const int Expected[] = {1, 2, 3, 4, 5, -4, -3, -2, -101, -1};
    return error.code == 11 && matches(Expected);
  }
}

static bool testBodyFailure() {
  reset();
  try {
    Composite value(2);
    (void)value;
    return false;
  } catch throws(std::error error) {
    const int Expected[] = {1, 2, 3, 4, 5, -5, -4, -3, -2, -101, -1};
    return error.code == 12 && matches(Expected);
  }
}

static bool testSuccess() {
  reset();
  try {
    {
      Composite value(0);
      const int Constructed[] = {1, 2, 3, 4, 5, 6};
      if (!matches(Constructed))
        return false;
    }
  } catch throws(std::error) {
    return false;
  }
  const int Expected[] = {1, 2, 3, 4, 5, 6, -106, -5, -4, -3, -2, -101, -1};
  return matches(Expected);
}

static bool testExplicitSuccessReturn() {
  reset();
  try {
    {
      Composite value(3);
      const int Constructed[] = {1, 2, 3, 4, 5};
      if (!matches(Constructed))
        return false;
    }
  } catch throws(std::error) {
    return false;
  }
  const int Expected[] = {1, 2, 3, 4, 5, -106, -5, -4, -3, -2, -101, -1};
  return matches(Expected);
}

static int ArrayConstructionIndex;
static int ArrayFailureIndex;

struct ArrayElement {
  int id;
  ArrayElement() throws : id(++ArrayConstructionIndex) {
    record(20 + id);
    if (id == ArrayFailureIndex)
      throw throws std::error{nullptr, 13};
  }
  ~ArrayElement() { record(-(20 + id)); }
};

struct ArrayHolder : TrackedBase {
  ArrayElement elements[3];
  ArrayHolder() throws = default;
  ~ArrayHolder() { record(-120); }
};

static bool testPartialArrayFailure() {
  reset();
  ArrayConstructionIndex = 0;
  ArrayFailureIndex = 2;
  try {
    ArrayHolder value;
    (void)value;
    return false;
  } catch throws(std::error error) {
    const int Expected[] = {1, 21, 22, -21, -101, -1};
    return error.code == 13 && matches(Expected);
  }
}

struct FallibleBase {
  explicit FallibleBase(bool Fail) throws {
    record(30);
    if (Fail)
      throw throws std::error{nullptr, 14};
  }
  ~FallibleBase() { record(-30); }
};

struct BaseHolder : TrackedBase, FallibleBase {
  BaseHolder() throws : TrackedBase(), FallibleBase(true) {}
};

static bool testBaseFailure() {
  reset();
  try {
    BaseHolder value;
    (void)value;
    return false;
  } catch throws(std::error error) {
    const int Expected[] = {1, 30, -101, -1};
    return error.code == 14 && matches(Expected);
  }
}

struct Delegating : TrackedBase {
  Tracked member;

  Delegating() throws : TrackedBase(), member(2) { record(7); }
  explicit Delegating(bool Fail) throws : Delegating() {
    record(8);
    if (Fail)
      throw throws std::error{nullptr, 15};
  }
  ~Delegating() { record(-107); }
};

static bool testDelegatingFailure() {
  reset();
  try {
    Delegating value(true);
    (void)value;
    return false;
  } catch throws(std::error error) {
    const int Expected[] = {1, 2, 7, 8, -107, -2, -101, -1};
    return error.code == 15 && matches(Expected);
  }
}

#ifdef ENABLE_LEGACY_EH
extern "C" void *__cxa_error_domain_itanium_exception_ptr() noexcept {
  return nullptr;
}

extern "C" unsigned long long __cxa_error_code_itanium_exception_ptr(void *) {
  return 16;
}

struct LegacyFailure {
  LegacyFailure() {
    record(40);
    throw 16;
  }
  ~LegacyFailure() { record(-40); }
};

struct LegacyHolder : TrackedBase {
  Tracked member;
  LegacyFailure failure;
  LegacyHolder() throws : TrackedBase(), member(2), failure() {}
};

static bool testLegacyFailure() {
  reset();
  try {
    LegacyHolder value;
    (void)value;
    return false;
  } catch throws(std::error error) {
    const int Expected[] = {1, 2, 40, -2, -101, -1};
    return error.code == 16 && matches(Expected);
  }
}
#endif

int main() {
  if (!testMemberFailure() || !testBodyFailure() || !testSuccess() ||
      !testExplicitSuccessReturn() || !testPartialArrayFailure() ||
      !testBaseFailure() || !testDelegatingFailure())
    return 1;
#ifdef ENABLE_LEGACY_EH
  if (!testLegacyFailure())
    return 2;
#endif
  return 0;
}
