// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++26 -fherbceptions -fms-extensions -fcxx-exceptions -fexceptions -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++26 -fherbceptions -fms-extensions -emit-llvm -o - %s | FileCheck %s --check-prefix=NOCXXEH

// SEH (__try/__except/__finally) combined with -fherbceptions on MSVC targets.
//
// A 'throws' function keeps its {T, i1} discriminated return while gaining the
// Windows EH personality needed by SEH. A 'throw throws' inside __try returns
// the error through the return channel (not by unwinding), while a real SEH
// exception that escapes the function is converted to std::error through the
// msvc_exception_ptr domain shims.

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
} // namespace std

int g;

// Plain SEH is unaffected by -fherbceptions.
// CHECK-LABEL: define dso_local noundef i32 @"?plain_seh_except@@YAHXZ"()
int plain_seh_except() {
  __try {
    g = 1;
    return 0;
  } __except (1) {
    return -1;
  }
}

// A 'throws' function containing SEH still returns {T, i1}.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @"?throwing_with_seh@@YAHXZ"()
// NOCXXEH-LABEL: define {{.*}} @"?throwing_with_seh@@YAHXZ"()
int throwing_with_seh() throws {
  __try {
    g = 4;
  } __except (1) {
    g = 5;
  }
  return 0;
}

// 'throw throws' inside __try: the error is materialized in the return slot;
// calls that build it unwind to the SEH catch.dispatch, and an SEH exception
// escaping the function becomes a std::error via the msvc_exception_ptr shims.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @"?throw_throws_in_seh@@YAHXZ"()
// CHECK-SAME:    personality ptr @__C_specific_handler
// CHECK:         catchswitch within none [label %__except] unwind label %[[LEGACY:[a-z0-9_.]+]]
// CHECK:       [[LEGACY]]:
// CHECK-NEXT:    catchswitch within none [label %herb.legacy.convert] unwind to caller
// CHECK:       __except:
// CHECK-NEXT:    catchpad within %{{.*}} [ptr null]
// CHECK-NEXT:    catchret from
// CHECK:         ret { { ptr, i64 }, i1 }
// CHECK:       herb.legacy.convert:
// CHECK-NEXT:    catchpad within %{{.*}} [ptr null]
// CHECK:         call ptr @__cxa_error_domain_msvc_exception_ptr()
// CHECK:         call i64 @__cxa_error_code_msvc_exception_ptr(
int throw_throws_in_seh() throws {
  __try {
    throw throws std::my_errc{42};
  } __except (1) {
    g = 6;
  }
  return 0;
}

// 'throw throws' inside a live __except handler exits the funclet via
// catchret, then returns {err, true} through the normal throws channel.
// CHECK-LABEL: define dso_local { { ptr, i64 }, i1 } @"?throw_in_except_live@@YAHXZ"()
// CHECK-SAME:    personality ptr @__C_specific_handler
// CHECK:       __except:
// CHECK-NEXT:    catchpad within %{{.*}} [ptr null]
// CHECK-NEXT:    catchret from %{{.*}} to label %__except{{[0-9]+}}
// CHECK:       __except{{[0-9]+}}:
// CHECK:         call i32 @llvm.eh.exceptioncode
// CHECK:         invoke {{.*}} @"?domain@?$error_domain@Umy_errc@std@@@std@@SAPEBUerror_domain_singleton@2@XZ"
// CHECK:         ret { { ptr, i64 }, i1 }
extern "C" void may_seh_fault();
int throw_in_except_live() throws {
  __try {
    may_seh_fault();
  } __except (1) {
    throw throws std::my_errc{9};
  }
  return 0;
}

// CHECK: attributes #{{[0-9]+}} = { {{.*}}throws{{.*}} }
