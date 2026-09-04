// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++20 \
// RUN:   -fherbceptions -fms-extensions -fcxx-exceptions -fexceptions \
// RUN:   -emit-llvm -o %t.x64.ll %s
// RUN: FileCheck %s --check-prefix=MS64-DEFAULT --input-file=%t.x64.ll
// RUN: FileCheck %s --check-prefix=MS64-COPY --input-file=%t.x64.ll
// RUN: FileCheck %s --check-prefix=MS64-VIRTUAL --input-file=%t.x64.ll
// RUN: FileCheck %s --check-prefix=MS64-INFERRED --input-file=%t.x64.ll
// RUN: FileCheck %s --check-prefix=MS64-DISABLED --input-file=%t.x64.ll
// RUN: opt -passes=verify -disable-output %t.x64.ll
// RUN: %clang_cc1 -triple i686-pc-windows-msvc -std=c++20 \
// RUN:   -fherbceptions -fms-extensions -fcxx-exceptions -fexceptions \
// RUN:   -emit-llvm -o %t.x86.ll %s
// RUN: opt -passes=verify -disable-output %t.x86.ll

// Microsoft constructors return `this`, whereas their default/copying
// closures ordinarily return void.  An active deterministic-failure channel
// must shape both callable boundaries independently: the direct constructor
// carries `this` on success, and the closure carries only its error payload.
// In particular, LLVM's `returned` parameter contract describes neither
// shaped result and would be invalid on both success/error alternatives.

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
};
} // namespace std

struct __declspec(dllexport) ActiveDefault {
  int value;
  ActiveDefault(int value = 7) throws : value(value) {}
};

// The direct definition and its `_F` default-argument closure both use the
// active ABI.  The closure must inspect the direct call's discriminator and
// propagate the error rather than silently returning void.
// MS64-DEFAULT-LABEL: define weak_odr dso_local dllexport { { ptr, i64 }, i1 } @"??0ActiveDefault@@QEAA@H@Z"(ptr noundef nonnull align 4 dereferenceable(4) %this,
// MS64-DEFAULT: ret { { ptr, i64 }, i1 }
// MS64-DEFAULT-LABEL: define weak_odr dso_local dllexport { { ptr, i64 }, i1 } @"??_FActiveDefault@@QEAAXXZ"(ptr %this)
// MS64-DEFAULT: %[[DEFAULT_CALL:.*]] = call { { ptr, i64 }, i1 } @"??0ActiveDefault@@QEAA@H@Z"({{.*}}i32 noundef 7)
// MS64-DEFAULT: %[[DEFAULT_DISC:.*]] = extractvalue { { ptr, i64 }, i1 } %[[DEFAULT_CALL]], 1
// MS64-DEFAULT: br i1 %[[DEFAULT_DISC]], label %herb.implicit.err, label %herb.implicit.ok
// MS64-DEFAULT: ret { { ptr, i64 }, i1 }

struct ActiveCopyClosure {
  int value;
  ActiveCopyClosure(ActiveCopyClosure &other, int extra = 17) throws
      : value(other.value + extra) {}
};

// Throwing a class by value makes the Microsoft EH metadata request the `_O`
// copying closure.  Although that runtime entry point has an ordinary void
// success result, its active constructor call must still be decoded exactly
// once and its failure payload returned through the closure's shaped ABI.
void force_copying_closure(ActiveCopyClosure &value) throws { throw value; }

// MS64-COPY-LABEL: define linkonce_odr dso_local { { ptr, i64 }, i1 } @"??_OActiveCopyClosure@@QEAAXAEAU0@@Z"(ptr %this, ptr %src)
// MS64-COPY: %[[COPY_CALL:.*]] = call { { ptr, i64 }, i1 } @"??0ActiveCopyClosure@@QEAA@AEAU0@H@Z"({{.*}}i32 noundef 17)
// MS64-COPY: %[[COPY_DISC:.*]] = extractvalue { { ptr, i64 }, i1 } %[[COPY_CALL]], 1
// MS64-COPY: br i1 %[[COPY_DISC]], label %herb.implicit.err, label %herb.implicit.ok
// MS64-COPY: ret { { ptr, i64 }, i1 }

struct VirtualBase {};

struct __declspec(dllexport) ActiveVirtual : virtual VirtualBase {
  int value;
  ActiveVirtual(int value = 19) throws : value(value) {}
};

// A virtual-base constructor has the ABI-only `is_most_derived` argument.
// It remains present on both the direct constructor and the `_F` closure when
// their return is shaped; changing the result must not shift or erase this
// hidden parameter.  A default closure constructs a complete object, hence it
// passes the established constant true value to the complete constructor.
// MS64-VIRTUAL-LABEL: define weak_odr dso_local dllexport { { ptr, i64 }, i1 } @"??0ActiveVirtual@@QEAA@H@Z"(ptr noundef nonnull align 8 dereferenceable(16) %this, i32 noundef %value, i32 noundef %is_most_derived)
// MS64-VIRTUAL-LABEL: define weak_odr dso_local dllexport { { ptr, i64 }, i1 } @"??_FActiveVirtual@@QEAAXXZ"(ptr %this, i32 %is_most_derived)
// MS64-VIRTUAL: store i32 %is_most_derived, ptr %{{.*}}, align 4
// MS64-VIRTUAL: %[[VIRTUAL_CALL:.*]] = call { { ptr, i64 }, i1 } @"??0ActiveVirtual@@QEAA@H@Z"({{.*}}i32 noundef 19, i32 noundef 1)
// MS64-VIRTUAL: %[[VIRTUAL_DISC:.*]] = extractvalue { { ptr, i64 }, i1 } %[[VIRTUAL_CALL]], 1
// MS64-VIRTUAL: br i1 %[[VIRTUAL_DISC]], label %herb.implicit.err, label %herb.implicit.ok

struct ActiveMember {
  int value;
  ActiveMember() throws;
  ActiveMember(const ActiveMember &) throws;
};

struct InferredSpecialMembers {
  ActiveMember member;
  InferredSpecialMembers() = default;
  InferredSpecialMembers(const InferredSpecialMembers &) = default;
};

void instantiate_inferred(const InferredSpecialMembers &source) throws {
  InferredSpecialMembers first;
  InferredSpecialMembers second(source);
}

// Implicit/defaulted special members inherit the live deterministic channel
// from their subobject operations.  Their Microsoft constructor definitions
// and the subobject calls must agree on the same shaped ABI.
// MS64-INFERRED-LABEL: define linkonce_odr dso_local { { ptr, i64 }, i1 } @"??0InferredSpecialMembers@@QEAA@XZ"(ptr noundef nonnull align 4 dereferenceable(4) %this)
// MS64-INFERRED: %[[INFERRED_DEFAULT_CALL:.*]] = call { { ptr, i64 }, i1 } @"??0ActiveMember@@QEAA@XZ"
// MS64-INFERRED: extractvalue { { ptr, i64 }, i1 } %[[INFERRED_DEFAULT_CALL]], 1
// MS64-INFERRED-LABEL: define linkonce_odr dso_local { { ptr, i64 }, i1 } @"??0InferredSpecialMembers@@QEAA@AEBU0@@Z"(ptr noundef nonnull align 4 dereferenceable(4) %this,
// MS64-INFERRED: %[[INFERRED_COPY_CALL:.*]] = call { { ptr, i64 }, i1 } @"??0ActiveMember@@QEAA@AEBU0@@Z"
// MS64-INFERRED: extractvalue { { ptr, i64 }, i1 } %[[INFERRED_COPY_CALL]], 1

struct __declspec(dllexport) DisabledDefault {
  int value;
  DisabledDefault(int value = 23) throws(false) : value(value) {}
};

// `throws(false)` is the ordinary non-failing ABI.  It therefore retains the
// direct constructor's pointer result and valid `returned(this)` contract,
// while its `_F` closure remains an ordinary void function.
// MS64-DISABLED-LABEL: define weak_odr dso_local dllexport noundef ptr @"??0DisabledDefault@@QEAA@H@Z"(ptr noundef nonnull returned align 4 dereferenceable(4) %this,
// MS64-DISABLED: ret ptr %{{.*}}
// MS64-DISABLED-LABEL: define weak_odr dso_local dllexport void @"??_FDisabledDefault@@QEAAXXZ"(ptr %this)
// MS64-DISABLED: call noundef ptr @"??0DisabledDefault@@QEAA@H@Z"({{.*}}i32 noundef 23)
// MS64-DISABLED: ret void

void instantiate_direct_calls() throws {
  ActiveDefault active;
  ActiveVirtual virtual_object;
  DisabledDefault disabled;
}
