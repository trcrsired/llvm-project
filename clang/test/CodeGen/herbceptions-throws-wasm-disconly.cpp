// RUN: %clang_cc1 -std=c++20 -triple wasm32-unknown-wasip1 -fherbceptions -emit-llvm %s -o %t.w32.ll
// RUN: FileCheck --input-file=%t.w32.ll --check-prefix=WASM %s
// RUN: llvm-as < %t.w32.ll -o /dev/null
// RUN: %clang_cc1 -std=c++20 -triple wasm64-unknown-wasip1 -fherbceptions -emit-llvm %s -o %t.w64.ll
// RUN: FileCheck --input-file=%t.w64.ll --check-prefix=WASM64 %s
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fherbceptions -emit-llvm %s -o %t.x86.ll
// RUN: FileCheck --input-file=%t.x86.ll --check-prefix=X86 %s

// WebAssembly has neither a flags register to carry the herbception
// discriminant out-of-band nor a multi-value calling convention in its
// default ABI, so a `throws` function returns just the i1 discriminant and
// the union{T,E} payload travels through a 'throws_sret' parameter:
//
//   i1 f(ptr throws_sret(union{T,E}), args...)
//
// The exception is an aggregate payload smaller than the error type: its
// destination object may be forwarded into the callee for guaranteed copy
// elision, and it is not large enough to hold E, so those keep the
// {union(T, E), i1} return. Other targets keep {union(T, E), i1}
// unconditionally.

namespace std {
struct error_domain_singleton {};
struct error {
  void *d;
  __SIZE_TYPE__ c;
};
struct my_errc { int v; };
template <class T> class error_domain;
template <> class error_domain<my_errc> {
public:
  static inline constexpr error_domain_singleton const *domain() noexcept;
  static inline __SIZE_TYPE__ code(my_errc) noexcept { return 0; }
};
}

struct Big { char data[32]; };
struct Tiny { char data[4]; };

// WASM: define i1 @_Z8f_scalari(ptr noalias writable throws_sret({ ptr, i32 }) align 4 %agg.result, i32 noundef %{{.*}})
// WASM: ret i1
// WASM64: define i1 @_Z8f_scalari(ptr noalias writable throws_sret({ ptr, i64 }) align 8 %agg.result, i32 noundef %{{.*}})
// WASM64: ret i1
// X86: define dso_local { { ptr, i64 }, i1 } @_Z8f_scalari(i32 noundef %{{.*}})
// X86: ret { { ptr, i64 }, i1 }
int f_scalar(int v) throws {
  if (v < 0) throw throws std::my_errc{v};
  return v;
}

// WASM: define i1 @_Z6f_voidi(ptr noalias writable throws_sret({ ptr, i32 }) align 4 %agg.result, i32 noundef %{{.*}})
// WASM64: define i1 @_Z6f_voidi(ptr noalias writable throws_sret({ ptr, i64 }) align 8 %agg.result, i32 noundef %{{.*}})
// X86: define dso_local { { ptr, i64 }, i1 } @_Z6f_voidi(i32 noundef %{{.*}})
void f_void(int v) throws {
  if (v < 0) throw throws std::my_errc{v};
}

// WASM: define i1 @_Z5f_bigi(ptr noalias writable throws_sret(%struct.Big) align 1 %agg.result, i32 noundef %{{.*}})
// WASM64: define i1 @_Z5f_bigi(ptr noalias writable throws_sret(%struct.Big) align 1 %agg.result, i32 noundef %{{.*}})
// X86: define dso_local { { ptr, i64 }, i1 } @_Z5f_bigi(ptr noalias writable throws_sret(%struct.Big) align 1 %agg.result, i32 noundef %{{.*}})
Big f_big(int v) throws {
  if (v < 0) throw throws std::my_errc{v};
  return {};
}

// Tiny is smaller than std::error, so the destination slot cannot hold an
// error and the {E, i1} form is kept even on wasm.
// WASM: define { { ptr, i32 }, i1 } @_Z6f_tinyi(i32 noundef %{{.*}})
// WASM64: define { { ptr, i64 }, i1 } @_Z6f_tinyi(i32 noundef %{{.*}})
// X86: define dso_local { { ptr, i64 }, i1 } @_Z6f_tinyi(i32 noundef %{{.*}})
Tiny f_tiny(int v) throws {
  if (v < 0) throw throws std::my_errc{v};
  return {};
}

// A throws constructor's `this` parameter is not marked 'returned' (the
// return value is the i1 discriminant). The linkonce_odr ctor definition is
// emitted at the end of the module; checked after the callers below.
struct Foo {
  int x;
  Foo(int v) throws : x(v) {}
};

// The caller reads the i1 discriminant from the call itself and the
// error/payload out of the throws_sret buffer.
// WASM-LABEL: define i1 @_Z13caller_scalari(
// WASM: %call = call i1 @_Z8f_scalari(ptr writable throws_sret({ ptr, i32 }) align 4 %tmp, i32 noundef %{{.*}})
// WASM: load { ptr, i32 }, ptr %tmp
// WASM: br i1 %call, label %try.err, label %try.ok
// WASM: try.ok:
// WASM: load i32, ptr %tmp
// WASM: ret i1
// X86-LABEL: define dso_local { { ptr, i64 }, i1 } @_Z13caller_scalari(
// X86: %call = call { { ptr, i64 }, i1 } @_Z8f_scalari(i32 noundef %{{.*}})
// X86: extractvalue { { ptr, i64 }, i1 } %call, 1
// X86: ret { { ptr, i64 }, i1 }
int caller_scalar(int v) throws {
  return try(f_scalar(v));
}

// A bool payload is stored as i8 in the union slot but is an i1 value: the
// caller must run the usual load conversion (icmp ne), not br on the i8.
// WASM-LABEL: define i1 @_Z13f_bool_calleri(
// WASM: %call = call i1 @_Z6f_booli(ptr writable throws_sret({ ptr, i32 }) align 4 %tmp, i32 noundef %{{.*}})
// WASM: br i1 %call, label %try.err, label %try.ok
// WASM: try.ok:
// WASM: load i8, ptr %tmp
// WASM: icmp ne i8 %{{.*}}, 0
// WASM-NOT: br i8
// WASM: ret i1
bool f_bool(int v) throws {
  if (v < 0) throw throws std::my_errc{v};
  return v > 0;
}
int f_bool_caller(int v) throws {
  if (try(f_bool(v)))
    return 1;
  return 0;
}

// WASM-LABEL: define i1 @_Z11caller_ctori(
// WASM: %call = call i1 @_ZN3FooC1Ei(ptr writable throws_sret({ ptr, i32 }) align 4 %tmp,
// WASM: br i1 %call,
void caller_ctor(int v) throws {
  Foo f(v);
  (void)f;
}

// WASM: define linkonce_odr i1 @_ZN3FooC2Ei(ptr noalias writable throws_sret({ ptr, i32 }) align 4 %agg.result, ptr noundef nonnull align 4 dereferenceable(4) %this, i32 noundef %{{.*}})
// WASM64: define linkonce_odr i1 @_ZN3FooC2Ei(ptr noalias writable throws_sret({ ptr, i64 }) align 8 %agg.result, ptr noundef nonnull align 4 dereferenceable(4) %this, i32 noundef %{{.*}})
// X86: define linkonce_odr { { ptr, i64 }, i1 } @_ZN3FooC2Ei(ptr noundef nonnull align 4 dereferenceable(4) %this, i32 noundef %{{.*}})
