// RUN: %clang_cc1 -std=c++20 -triple wasm32-unknown-wasip1 -fherbceptions -emit-llvm %s -o %t.w32.ll
// RUN: FileCheck --input-file=%t.w32.ll --check-prefix=WASM %s
// RUN: llvm-as < %t.w32.ll -o /dev/null
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fherbceptions -emit-llvm %s -o %t.x86.ll
// RUN: FileCheck --input-file=%t.x86.ll --check-prefix=X86 %s
// RUN: llvm-as < %t.x86.ll -o /dev/null

// A throws call in the default argument of a throws function template is
// wrapped in an implicit try() when the default argument is instantiated --
// in the *declared* function's context. The try node is then emitted in the
// caller's frame, which need not be a throws function: in main() the error
// traps, in a `catch throws` scope it reaches the handler, and in a throws
// function it auto-propagates. Emitting it must not touch the caller's
// (nonexistent) herbception return slots.

namespace std {
struct error {
  void *d;
  __SIZE_TYPE__ c;
};
}

bool daylight() throws;

template <long X>
int local(int ts, bool dl = daylight()) throws {
  return ts;
}

// The bare call's default-arg error traps in main; the call inside try{}
// routes to the catch throws handler instead.
// WASM-LABEL: define noundef i32 @main()
// WASM: %call = call i1 @_Z8daylightv(ptr {{.*}}throws_sret({ ptr, i32 })
// WASM: br i1 %call, label %try.err, label %try.ok
// WASM: try.err:
// WASM-NEXT: call void @llvm.trap()
// WASM-NEXT: unreachable
// WASM: %call{{[0-9]+}} = call i1 @_Z8daylightv(ptr {{.*}}throws_sret({ ptr, i32 })
// WASM: br i1 %{{.*}}, label %try.err{{[0-9]+}}, label %try.ok{{[0-9]+}}
// WASM: try.err{{[0-9]+}}:
// WASM-NOT: call void @llvm.trap()
// WASM: store %"struct.std::error"
// X86-LABEL: define dso_local noundef i32 @main()
// X86: call { { ptr, i64 }, i1 } @_Z8daylightv()
// X86: try.err:
// X86-NEXT: call void @llvm.trap()
// X86-NEXT: unreachable
int main() {
  int r = local<0>(0);
  try {
    r += local<0>(0);
  } catch throws(std::error e) {
    r = -1;
  }
  return r;
}

// In a throws caller the same default argument auto-propagates.
// WASM-LABEL: define i1 @_Z12caller_outerv(
// WASM: %call = call i1 @_Z8daylightv(ptr {{.*}}throws_sret({ ptr, i32 })
// WASM: try.err:
// WASM-NOT: call void @llvm.trap()
// WASM: store i1 true, ptr %herbception.disc
// WASM: ret i1
// X86-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z12caller_outerv(
// X86: try.err:
// X86-NOT: call void @llvm.trap()
// X86: ret { { ptr, i64 }, i1 }
int caller_outer() throws {
  return local<0>(0);
}
