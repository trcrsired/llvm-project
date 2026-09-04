// clang-format off
// RUN: %clang -std=c++20 -fherbceptions -fno-exceptions -O0 %s -o %t.noeh.o0
// RUN: %t.noeh.o0
// RUN: %clang -std=c++20 -fherbceptions -fno-exceptions -O2 %s -o %t.noeh.o2
// RUN: %t.noeh.o2
// RUN: %clang -std=c++20 -fherbceptions -fexceptions -DENABLE_LEGACY_EH -O0 %s -o %t.eh.o0
// RUN: %t.eh.o0
// RUN: %clang -std=c++20 -fherbceptions -fexceptions -DENABLE_LEGACY_EH -O2 %s -o %t.eh.o2
// RUN: %t.eh.o2
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
extern "C" unsigned long long
__cxa_error_code_itanium_exception_ptr(void *);
#endif

static int Events[64];
static int EventCount;
static int FailureMode;

static void record(int Event) { Events[EventCount++] = Event; }

template <__SIZE_TYPE__ N> static bool matches(const int (&Expected)[N]) {
  if (EventCount != static_cast<int>(N))
    return false;
  for (__SIZE_TYPE__ I = 0; I != N; ++I)
    if (Events[I] != Expected[I])
      return false;
  return true;
}

static void reset(int Mode) {
  EventCount = 0;
  FailureMode = Mode;
}

struct VariadicBase {
  VariadicBase(int Mode, ...) throws {
    record(10);
    if (Mode == 1)
      throw throws std::error{nullptr, 101};
  }
  ~VariadicBase() { record(-10); }
};

struct CompletedMember {
  CompletedMember() noexcept { record(20); }
  ~CompletedMember() { record(-20); }
};

struct FallibleMember {
  FallibleMember() throws {
    record(30);
    if (FailureMode == 2)
      throw throws std::error{nullptr, 102};
  }
  ~FallibleMember() { record(-30); }
};

// A variadic inherited constructor cannot forward its arguments through a
// standalone ABI thunk, so Clang emits its prologue inline in the caller.
struct Derived : VariadicBase {
  using VariadicBase::VariadicBase;
  CompletedMember completed;
  FallibleMember fallible;
};

__attribute__((noinline)) static int constructActive(int Mode) throws {
  FailureMode = Mode;
  Derived value(Mode, 7);
  return 77;
}

static bool testActiveMemberFailure() {
  reset(2);
  try {
    (void)constructActive(2);
    return false;
  } catch throws(std::error Error) {
    const int Expected[] = {10, 20, 30, -20, -10};
    return Error.code == 102 && matches(Expected);
  }
}

static bool testPlainCatchMemberFailure() {
  reset(2);
  try {
    Derived value(2, 7);
    (void)value;
    return false;
  } catch throws(std::error Error) {
    const int Expected[] = {10, 20, 30, -20, -10};
    return Error.code == 102 && matches(Expected);
  }
}

static bool testBaseFailure() {
  reset(1);
  try {
    Derived value(1, 7);
    (void)value;
    return false;
  } catch throws(std::error Error) {
    const int Expected[] = {10};
    return Error.code == 101 && matches(Expected);
  }
}

static bool testSuccess() {
  reset(0);
  try {
    if (constructActive(0) != 77)
      return false;
  } catch throws(std::error) {
    return false;
  }
  const int Expected[] = {10, 20, 30, -30, -20, -10};
  return matches(Expected);
}

struct Root {
  Root(int Mode, ...) throws {
    record(40);
    if (Mode == 4)
      throw throws std::error{nullptr, 104};
  }
  ~Root() { record(-40); }
};

struct MiddleMember {
  MiddleMember() throws {
    record(50);
    if (FailureMode == 5)
      throw throws std::error{nullptr, 105};
  }
  ~MiddleMember() { record(-50); }
};

struct Middle : Root {
  using Root::Root;
  MiddleMember middle;
};

struct LeafMember {
  LeafMember() throws {
    record(60);
    if (FailureMode == 6)
      throw throws std::error{nullptr, 106};
  }
  ~LeafMember() { record(-60); }
};

// This second inheritance step nests two inlined inherited-constructor
// scopes. Each scope must retain its own completion boundary.
struct Leaf : Middle {
  using Middle::Middle;
  LeafMember leaf;
};

__attribute__((noinline)) static void constructNestedActive(int Mode) throws {
  FailureMode = Mode;
  Leaf value(Mode, 9);
}

static bool testNestedInnerFailureInPlainCatch() {
  reset(5);
  try {
    Leaf value(5, 9);
    (void)value;
    return false;
  } catch throws(std::error Error) {
    const int Expected[] = {40, 50, -40};
    return Error.code == 105 && matches(Expected);
  }
}

static bool testNestedOuterFailureInActiveCaller() {
  reset(6);
  try {
    constructNestedActive(6);
    return false;
  } catch throws(std::error Error) {
    const int Expected[] = {40, 50, 60, -50, -40};
    return Error.code == 106 && matches(Expected);
  }
}

#ifdef ENABLE_LEGACY_EH
extern "C" void *__cxa_error_domain_itanium_exception_ptr() noexcept {
  return nullptr;
}

extern "C" unsigned long long
__cxa_error_code_itanium_exception_ptr(void *) {
  return 109;
}

struct LegacyBase {
  LegacyBase(int, ...) throws { record(70); }
  ~LegacyBase() { record(-70); }
};

struct LegacyCompletedMember {
  LegacyCompletedMember() noexcept { record(80); }
  ~LegacyCompletedMember() { record(-80); }
};

struct LegacyFailure {
  LegacyFailure() {
    record(90);
    throw 109;
  }
  ~LegacyFailure() { record(-90); }
};

struct LegacyDerived : LegacyBase {
  using LegacyBase::LegacyBase;
  LegacyCompletedMember completed;
  LegacyFailure failure;
};

// The inlined deterministic-failure cleanups are NormalCleanup-only. A legacy
// exception must therefore use the separate EH cleanups and destroy each
// completed subobject exactly once.
__attribute__((noinline)) static int constructLegacy() throws {
  try {
    LegacyDerived value(0, 11);
    (void)value;
    return 0;
  } catch (int Code) {
    return Code;
  }
}

static bool testLegacyCleanupIsIndependent() {
  reset(0);
  try {
    if (constructLegacy() != 109)
      return false;
  } catch throws(std::error) {
    return false;
  }
  const int Expected[] = {70, 80, 90, -80, -70};
  return matches(Expected);
}
#endif

int main() {
  if (!testActiveMemberFailure())
    return 1;
  if (!testPlainCatchMemberFailure())
    return 2;
  if (!testBaseFailure())
    return 3;
  if (!testSuccess())
    return 4;
  if (!testNestedInnerFailureInPlainCatch())
    return 5;
  if (!testNestedOuterFailureInActiveCaller())
    return 6;
#ifdef ENABLE_LEGACY_EH
  if (!testLegacyCleanupIsIndependent())
    return 7;
#endif
  return 0;
}
