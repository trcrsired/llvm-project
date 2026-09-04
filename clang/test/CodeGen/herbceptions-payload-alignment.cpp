// RUN: %clang --target=x86_64-unknown-linux-gnu -std=c++20 \
// RUN:   -fherbceptions -fno-exceptions -S -emit-llvm -o - %s \
// RUN:   | FileCheck %s
// RUN: %clang --target=x86_64-unknown-linux-gnu -std=c++20 \
// RUN:   -fherbceptions -fno-exceptions -S -emit-llvm -o - %s \
// RUN:   | opt -passes=verify -disable-output

// A shaped return payload is bytewise union storage for the success and error
// alternatives. Its LLVM carrier must therefore satisfy both the maximum ABI
// size and the maximum ABI alignment, independently.

struct PackedEight {
  char bytes[8];
};
using NaturallyAlignedEight = long long;
static_assert(sizeof(PackedEight) == sizeof(NaturallyAlignedEight));
static_assert(alignof(PackedEight) == 1);
static_assert(alignof(NaturallyAlignedEight) == 8);

// Use a naturally-aligned scalar because LLVM DataLayout determines carrier
// alignment from the lowered type. Equal-size alternatives must select the
// more-aligned carrier rather than whichever alternative appears first.
// CHECK-LABEL: define dso_local { i64, i1 }
PackedEight equal_size(bool fail) return_failure{NaturallyAlignedEight} {
  if (fail)
    return_failure NaturallyAlignedEight{};
  return PackedEight{};
}

struct LargePacked {
  char bytes[16];
};
static_assert(sizeof(LargePacked) == 16);
static_assert(alignof(LargePacked) == 1);

// When neither alternative dominates both dimensions, the carrier starts
// with the maximally-aligned member and adds a byte tail for the maximum size.
// CHECK-LABEL: define dso_local { { i64, [8 x i8] }, i1 }
LargePacked cross_dominant(bool fail) return_failure{NaturallyAlignedEight} {
  if (fail)
    return_failure NaturallyAlignedEight{};
  return LargePacked{};
}
