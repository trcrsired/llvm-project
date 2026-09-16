; The pass runs in the default optimization pipeline at every non-O0 level,
; in the ThinLTO pre-link pipeline (so -flto=thin compiles fold throws while
; the conversion helpers are still plain call sites and cannot be dissolved
; by post-link importing/inlining), and in the ThinLTO post-link pipeline.
; It can be disabled with -enable-herbceptions-legacy-eh-fold=false.
;
; RUN: opt -passes='default<O1>' -S < %s | FileCheck %s
; RUN: opt -passes='default<O2>' -S < %s | FileCheck %s
; RUN: opt -passes='default<O3>' -S < %s | FileCheck %s
; RUN: opt -passes='thinlto-pre-link<O2>' -S < %s | FileCheck %s
; RUN: opt -passes='thinlto<O2>' -S < %s | FileCheck %s
; RUN: opt -enable-herbceptions-legacy-eh-fold=false -passes='thinlto-pre-link<O2>' -S < %s | FileCheck %s --check-prefix=DISABLED
; RUN: opt -enable-herbceptions-legacy-eh-fold=false -passes='default<O3>' -S < %s | FileCheck %s --check-prefix=DISABLED

target triple = "x86_64-unknown-linux-gnu"

@_ZTIi = external constant ptr

; CHECK-LABEL: define {{.*}} @foo
; CHECK-NOT: __cxa_throw
; CHECK: call i64 @__cxa_error_code_itanium_exception_ptr(i64 2,
; CHECK: ret
;
; DISABLED-LABEL: define {{.*}} @foo
; DISABLED: invoke void @__cxa_throw
; DISABLED-NOT: exception_ptr(i64 2
; DISABLED: ret
define dso_local { { ptr, i64 }, i1 } @foo() personality ptr @__gxx_personality_v0 {
entry:
  %obj = tail call ptr @__cxa_allocate_exception(i64 4)
  invoke void @__cxa_throw(ptr nonnull %obj, ptr nonnull @_ZTIi, ptr null)
          to label %dead unwind label %throw.lpad

dead:
  unreachable

throw.lpad:
  %lp = landingpad { ptr, i32 }
          catch ptr null
  %exn = extractvalue { ptr, i32 } %lp, 0
  %dom = tail call ptr @__cxa_error_domain_itanium_exception_ptr()
  %code = tail call i64 @__cxa_error_code_itanium_exception_ptr(i64 1, ptr noundef %exn, ptr null, ptr null)
  %e0 = insertvalue { ptr, i64 } poison, ptr %dom, 0
  %e1 = insertvalue { ptr, i64 } %e0, i64 %code, 1
  %r0 = insertvalue { { ptr, i64 }, i1 } poison, { ptr, i64 } %e1, 0
  %r1 = insertvalue { { ptr, i64 }, i1 } %r0, i1 true, 1
  ret { { ptr, i64 }, i1 } %r1
}

declare ptr @__gxx_personality_v0(...)
declare ptr @__cxa_allocate_exception(i64)
declare void @__cxa_throw(ptr, ptr, ptr)
declare ptr @__cxa_error_domain_itanium_exception_ptr()
declare i64 @__cxa_error_code_itanium_exception_ptr(i64, ptr, ptr, ptr)
