// clang-format off
// RUN: %clang -std=c++20 -O0 -fherbceptions -fexceptions -S -emit-llvm -o - %s | opt -passes=verify -disable-output
// RUN: %clangxx -std=c++20 -O0 -fherbceptions -fexceptions %s -o %t.o0
// RUN: %t.o0
// RUN: %clangxx -std=c++20 -O2 -fherbceptions -fexceptions %s -o %t.o2
// RUN: %t.o2
// REQUIRES: native
// clang-format on

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
};
} // namespace std

struct LegacyA {};
struct LegacyB {};

static int CleanupCount;

struct Guard {
  ~Guard() { ++CleanupCount; }
};

void fail(__SIZE_TYPE__ Code) throws {
  throw throws std::error{nullptr, Code};
}

static bool bareCallSelectsFirstHandler() {
  int Selected = 0;
  CleanupCount = 0;
  try {
    Guard G;
    fail(11);
  } catch throws(std::error Error) {
    Selected = Error.code == 11 ? 1 : 101;
  } catch throws(std::error Error) {
    Selected = Error.code == 11 ? 2 : 102;
  }
  return Selected == 1 && CleanupCount == 1;
}

static bool directThrowSelectsFirstHandler() {
  int Selected = 0;
  CleanupCount = 0;
  try {
    Guard G;
    throw throws std::error{nullptr, 12};
  } catch throws(std::error Error) {
    Selected = Error.code == 12 ? 1 : 101;
  } catch throws(std::error Error) {
    Selected = Error.code == 12 ? 2 : 102;
  }
  return Selected == 1 && CleanupCount == 1;
}

static bool traditionalBeforeFirstRoutesToFirst() {
  int Selected = 0;
  CleanupCount = 0;
  try {
    throw LegacyA{};
  } catch (LegacyA &) {
    Guard G;
    fail(21);
  } catch throws(std::error Error) {
    Selected = Error.code == 21 ? 1 : 101;
  } catch (LegacyB &) {
    Selected = 102;
  } catch throws(std::error Error) {
    Selected = Error.code == 21 ? 2 : 103;
  }
  return Selected == 1 && CleanupCount == 1;
}

static bool traditionalBetweenHandlersRoutesToSecond() {
  int Selected = 0;
  CleanupCount = 0;
  try {
    throw LegacyB{};
  } catch (LegacyA &) {
    Selected = 101;
  } catch throws(std::error Error) {
    Selected = Error.code == 22 ? 1 : 102;
  } catch (LegacyB &) {
    Guard G;
    fail(22);
  } catch throws(std::error Error) {
    Selected = Error.code == 22 ? 2 : 103;
  }
  return Selected == 2 && CleanupCount == 1;
}

int main() {
  if (!bareCallSelectsFirstHandler())
    return 1;
  if (!directThrowSelectsFirstHandler())
    return 2;
  if (!traditionalBeforeFirstRoutesToFirst())
    return 3;
  if (!traditionalBetweenHandlersRoutesToSecond())
    return 4;
  return 0;
}
