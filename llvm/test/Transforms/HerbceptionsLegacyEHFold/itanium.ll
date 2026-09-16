; RUN: opt -passes=herbceptions-legacy-eh-fold -S < %s | FileCheck %s
;
; Itanium model: a __cxa_throw whose unwind edge reaches only the
; legacy->std::error conversion is folded into a direct conversion call.
; The constructor-failure edge into the shared conversion block must be
; preserved.

target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [3 x i8] c"eh\00", align 1
@_ZTISt13runtime_error = external constant ptr
@_ZTIi = external constant ptr

; CHECK-LABEL: define {{.*}} @foo
define dso_local { { ptr, i64 }, i1 } @foo() local_unnamed_addr personality ptr @__gxx_personality_v0 {
entry:
  %obj = tail call ptr @__cxa_allocate_exception(i64 16)
  invoke void @_ZNSt13runtime_errorC1EPKc(ptr noundef nonnull align 8 dereferenceable(16) %obj, ptr noundef nonnull @.str)
          to label %ctor.ok unwind label %ctor.fail

ctor.ok:
; The throw becomes a direct conversion (flags==2) and a plain branch;
; no unwind.
; CHECK-NOT: __cxa_throw
; CHECK: %[[DOM:.*]] = call ptr @__cxa_error_domain_itanium_exception_ptr()
; CHECK: %[[CODE:.*]] = call i64 @__cxa_error_code_itanium_exception_ptr(i64 2, ptr %obj, ptr @_ZTISt13runtime_error, ptr @_ZNSt13runtime_errorD1Ev)
; CHECK: br label %[[CONT:.*]]
  invoke void @__cxa_throw(ptr nonnull %obj, ptr nonnull @_ZTISt13runtime_error, ptr nonnull @_ZNSt13runtime_errorD1Ev)
          to label %dead unwind label %throw.lpad

dead:
  unreachable

; Ctor-failure path still converts the in-flight exception the slow way
; (flags==1).
; CHECK: landingpad
; CHECK: call i64 @__cxa_error_code_itanium_exception_ptr(i64 1,
; CHECK: [[CONT]]:
; CHECK: phi ptr {{.*}}%[[DOM]]
; CHECK: phi i64 {{.*}}%[[CODE]]
ctor.fail:
  %lp0 = landingpad { ptr, i32 }
          catch ptr null
  tail call void @__cxa_free_exception(ptr nonnull %obj)
  br label %conv

throw.lpad:
  %lp1 = landingpad { ptr, i32 }
          catch ptr null
  br label %conv

conv:
  %exn.agg = phi { ptr, i32 } [ %lp1, %throw.lpad ], [ %lp0, %ctor.fail ]
  %exn = extractvalue { ptr, i32 } %exn.agg, 0
  %dom = tail call ptr @__cxa_error_domain_itanium_exception_ptr()
  %code = tail call i64 @__cxa_error_code_itanium_exception_ptr(i64 1, ptr noundef %exn, ptr null, ptr null)
  %e0 = insertvalue { ptr, i64 } poison, ptr %dom, 0
  %e1 = insertvalue { ptr, i64 } %e0, i64 %code, 1
  %r0 = insertvalue { { ptr, i64 }, i1 } poison, { ptr, i64 } %e1, 0
  %r1 = insertvalue { { ptr, i64 }, i1 } %r0, i1 true, 1
  ret { { ptr, i64 }, i1 } %r1
}

; A throw whose exception is observed by a real catch must not be folded.
; CHECK-LABEL: define {{.*}} @real_catch
; CHECK: invoke void @__cxa_throw
; CHECK-NOT: exception_ptr(i64 2
define i32 @real_catch() personality ptr @__gxx_personality_v0 {
  %obj = tail call ptr @__cxa_allocate_exception(i64 4)
  invoke void @__cxa_throw(ptr %obj, ptr @_ZTIi, ptr null)
          to label %dead unwind label %lpad
lpad:
  %lp = landingpad { ptr, i32 }
          catch ptr null
  %exn = extractvalue { ptr, i32 } %lp, 0
  %b = tail call ptr @__cxa_begin_catch(ptr %exn)
  tail call void @__cxa_end_catch()
  ret i32 7
dead:
  unreachable
}

; A throw unwinding to caller (cleanup + resume) must not be folded.
; CHECK-LABEL: define {{.*}} @to_caller
; CHECK: invoke void @__cxa_throw
; CHECK-NOT: exception_ptr(i64 2
define void @to_caller() personality ptr @__gxx_personality_v0 {
  %obj = tail call ptr @__cxa_allocate_exception(i64 4)
  invoke void @__cxa_throw(ptr %obj, ptr @_ZTIi, ptr null)
          to label %dead unwind label %lpad
lpad:
  %lp = landingpad { ptr, i32 }
          cleanup
  resume { ptr, i32 } %lp
dead:
  unreachable
}

; A landingpad with a typed catch clause sharing the dispatch must not be
; folded even if the catch-all conversion path also exists.
; CHECK-LABEL: define {{.*}} @typed_sibling
; CHECK: invoke void @__cxa_throw
; CHECK-NOT: exception_ptr(i64 2
; CHECK: ret i32
define i32 @typed_sibling() personality ptr @__gxx_personality_v0 {
  %obj = tail call ptr @__cxa_allocate_exception(i64 4)
  invoke void @__cxa_throw(ptr %obj, ptr @_ZTIi, ptr null)
          to label %dead unwind label %lpad
lpad:
  %lp = landingpad { ptr, i32 }
          catch ptr @_ZTIi
          catch ptr null
  %exn = extractvalue { ptr, i32 } %lp, 0
  %dom = tail call ptr @__cxa_error_domain_itanium_exception_ptr()
  %code = tail call i64 @__cxa_error_code_itanium_exception_ptr(i64 1, ptr noundef %exn, ptr null, ptr null)
  ret i32 0
dead:
  unreachable
}

declare ptr @__gxx_personality_v0(...)
declare ptr @__cxa_allocate_exception(i64)
declare void @__cxa_throw(ptr, ptr, ptr)
declare void @__cxa_free_exception(ptr)
declare ptr @__cxa_begin_catch(ptr)
declare void @__cxa_end_catch()
declare void @_ZNSt13runtime_errorC1EPKc(ptr, ptr)
declare void @_ZNSt13runtime_errorD1Ev(ptr)
declare ptr @__cxa_error_domain_itanium_exception_ptr()
declare i64 @__cxa_error_code_itanium_exception_ptr(i64, ptr, ptr, ptr)
