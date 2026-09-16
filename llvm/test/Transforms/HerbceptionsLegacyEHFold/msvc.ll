; RUN: opt -passes=herbceptions-legacy-eh-fold -S < %s | FileCheck %s
;
; MSVC model: a _CxxThrowException whose unwind edge reaches only the
; legacy->std::error conversion catchpad is folded into a direct
; conversion call; the dead catchswitch is cleaned up.

target triple = "x86_64-unknown-windows-msvc"

@"_TI2?AVruntime_error@std@@" = external constant ptr

; CHECK-LABEL: define {{.*}} @"?foo@@YAXXZ"
define dso_local { { ptr, i64 }, i1 } @"?foo@@YAXXZ"() local_unnamed_addr personality ptr @__CxxFrameHandler3 {
entry:
  %obj = alloca i8, i32 24, align 8
; The throw becomes a direct conversion and a plain branch; no unwind,
; and the now-dead catchswitch dispatch is removed entirely.
; CHECK-NOT: _CxxThrowException
; CHECK-NOT: catchswitch
; CHECK: %[[DOM:.*]] = call ptr @__cxa_error_domain_msvc_exception_ptr()
; CHECK: %[[CODE:.*]] = call i64 @__cxa_error_code_msvc_exception_ptr(i64 2, ptr %obj, ptr @"_TI2?AVruntime_error@std@@")
; CHECK: br label %[[CONT:.*]]
; CHECK: [[CONT]]:
; CHECK: ret
  invoke void @_CxxThrowException(ptr %obj, ptr @"_TI2?AVruntime_error@std@@")
          to label %dead unwind label %cs

dead:
  unreachable

cs:
  %cswtok = catchswitch within none [label %pad] unwind to caller

pad:
  %cp = catchpad within %cswtok [ptr null, i32 0, ptr null]
  %dom = call ptr @__cxa_error_domain_msvc_exception_ptr() [ "funclet"(token %cp) ]
  %code = call i64 @__cxa_error_code_msvc_exception_ptr(i64 1, ptr null, ptr null) [ "funclet"(token %cp) ]
  catchret from %cp to label %cont

cont:
  %e0 = insertvalue { ptr, i64 } poison, ptr %dom, 0
  %e1 = insertvalue { ptr, i64 } %e0, i64 %code, 1
  %r0 = insertvalue { { ptr, i64 }, i1 } poison, { ptr, i64 } %e1, 0
  %r1 = insertvalue { { ptr, i64 }, i1 } %r0, i1 true, 1
  ret { { ptr, i64 }, i1 } %r1
}

; A catchswitch with a sibling handler must not be folded.
; CHECK-LABEL: define {{.*}} @typed_sibling
; CHECK: invoke void @_CxxThrowException
; CHECK-NOT: exception_ptr(i64 2
; CHECK: ret
define i32 @typed_sibling() personality ptr @__CxxFrameHandler3 {
entry:
  %obj = alloca i8, i32 24, align 8
  invoke void @_CxxThrowException(ptr %obj, ptr @"_TI2?AVruntime_error@std@@")
          to label %dead unwind label %cs

dead:
  unreachable

cs:
  %cswtok = catchswitch within none [label %pad, label %other] unwind to caller

pad:
  %cp = catchpad within %cswtok [ptr null, i32 0, ptr null]
  %dom = call ptr @__cxa_error_domain_msvc_exception_ptr() [ "funclet"(token %cp) ]
  %code = call i64 @__cxa_error_code_msvc_exception_ptr(i64 1, ptr null, ptr null) [ "funclet"(token %cp) ]
  catchret from %cp to label %cont

other:
  %op = catchpad within %cswtok [ptr null, i32 0, ptr null]
  catchret from %op to label %cont

cont:
  ret i32 0
}

; A MSVC rethrow (_CxxThrowException(nullptr, nullptr)) is not a fresh
; throw and must not be folded.
; CHECK-LABEL: define {{.*}} @rethrow
; CHECK: invoke void @_CxxThrowException
; CHECK-NOT: exception_ptr(i64 2
; CHECK: ret
define i32 @rethrow() personality ptr @__CxxFrameHandler3 {
entry:
  invoke void @_CxxThrowException(ptr null, ptr null)
          to label %dead unwind label %cs

dead:
  unreachable

cs:
  %cswtok = catchswitch within none [label %pad] unwind to caller

pad:
  %cp = catchpad within %cswtok [ptr null, i32 0, ptr null]
  %dom = call ptr @__cxa_error_domain_msvc_exception_ptr() [ "funclet"(token %cp) ]
  %code = call i64 @__cxa_error_code_msvc_exception_ptr(i64 1, ptr null, ptr null) [ "funclet"(token %cp) ]
  catchret from %cp to label %cont

cont:
  ret i32 0
}

; The continuation may itself contain a phi taking the conversion results
; (e.g. several conversion sites merging). That phi must keep the original
; call result for the conversion edge and gain a new incoming value per
; folded site. The rethrow invoke keeps the pad alive so the phi survives.
; CHECK-LABEL: define {{.*}} @edge_phi
; CHECK: %[[DOM:.*]] = call ptr @__cxa_error_domain_msvc_exception_ptr()
; CHECK: %[[CODE:.*]] = call i64 @__cxa_error_code_msvc_exception_ptr(i64 2, ptr %obj, ptr @"_TI2?AVruntime_error@std@@")
; CHECK: br label %[[CONT:.*]]
; CHECK: invoke void @_CxxThrowException(ptr null, ptr null)
; CHECK: [[CONT]]:
; CHECK: phi ptr [ %dom, %pad ], [ %[[DOM]], %throw ]
; CHECK: phi i64 [ %code, %pad ], [ %[[CODE]], %throw ]
; CHECK: ret
define i32 @edge_phi(i1 %c) personality ptr @__CxxFrameHandler3 {
entry:
  %obj = alloca i8, i32 24, align 8
  br i1 %c, label %throw, label %rethrow

throw:
  invoke void @_CxxThrowException(ptr %obj, ptr @"_TI2?AVruntime_error@std@@")
          to label %dead unwind label %cs

rethrow:
  invoke void @_CxxThrowException(ptr null, ptr null)
          to label %dead unwind label %cs

dead:
  unreachable

cs:
  %cswtok = catchswitch within none [label %pad] unwind to caller

pad:
  %cp = catchpad within %cswtok [ptr null, i32 0, ptr null]
  %dom = call ptr @__cxa_error_domain_msvc_exception_ptr() [ "funclet"(token %cp) ]
  %code = call i64 @__cxa_error_code_msvc_exception_ptr(i64 1, ptr null, ptr null) [ "funclet"(token %cp) ]
  catchret from %cp to label %cont

cont:
  %pd = phi ptr [ %dom, %pad ]
  %pc = phi i64 [ %code, %pad ]
  %e0 = insertvalue { ptr, i64 } poison, ptr %pd, 0
  %e1 = insertvalue { ptr, i64 } %e0, i64 %pc, 1
  %r0 = insertvalue { { ptr, i64 }, i1 } poison, { ptr, i64 } %e1, 0
  %r1 = insertvalue { { ptr, i64 }, i1 } %r0, i1 true, 1
  %p = extractvalue { { ptr, i64 }, i1 } %r1, 1
  %z = zext i1 %p to i32
  ret i32 %z
}

declare void @__CxxFrameHandler3(...)
declare void @_CxxThrowException(ptr, ptr)
declare ptr @__cxa_error_domain_msvc_exception_ptr()
declare i64 @__cxa_error_code_msvc_exception_ptr(i64, ptr, ptr)
