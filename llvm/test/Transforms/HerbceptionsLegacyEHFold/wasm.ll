; RUN: opt -passes=herbceptions-legacy-eh-fold -S < %s | FileCheck %s
;
; Wasm EH model (catchswitch/catchpad/catchret like MSVC, but the
; conversion uses the Itanium entry points on the wasm.get.exception
; value). The ctor-failure cleanup edge shares the catchswitch and must
; be preserved.

target triple = "wasm32-unknown-wasip1"

@_ZTISt13runtime_error = external constant ptr

; CHECK-LABEL: define {{.*}} @foo
define hidden { { ptr, i32 }, i1 } @foo() local_unnamed_addr personality ptr @__gxx_wasm_personality_v0 {
entry:
  %obj = tail call ptr @__cxa_allocate_exception(i32 8)
  invoke ptr @_ZNSt13runtime_errorC1EPKc(ptr noundef nonnull align 4 dereferenceable(8) %obj, ptr noundef nonnull null)
          to label %ctor.ok unwind label %ctor.cleanup

ctor.ok:
; CHECK-NOT: __cxa_throw
; CHECK: %[[DOM:.*]] = call ptr @__cxa_error_domain_itanium_exception_ptr()
; CHECK: %[[CODE:.*]] = call i32 @__cxa_error_code_itanium_exception_ptr_direct(ptr %obj, ptr @_ZTISt13runtime_error, ptr @_ZNSt13runtime_errorD1Ev)
; CHECK: br label %[[CONT:.*]]
  invoke void @__cxa_throw(ptr nonnull %obj, ptr nonnull @_ZTISt13runtime_error, ptr nonnull @_ZNSt13runtime_errorD1Ev)
          to label %dead unwind label %cs

dead:
  unreachable

; The catchswitch stays alive for the ctor-failure edge and keeps its
; real catchpad conversion.
; CHECK: catchswitch
; CHECK: catchpad
; CHECK: call i32 @__cxa_error_code_itanium_exception_ptr(
; CHECK: catchret
; CHECK: [[CONT]]:
; CHECK: phi ptr {{.*}}%[[DOM]]
; CHECK: phi i32 {{.*}}%[[CODE]]
cs:
  %cswtok = catchswitch within none [label %pad] unwind to caller

pad:
  %cp = catchpad within %cswtok [ptr null]
  %exn = tail call ptr @llvm.wasm.get.exception(token %cp)
  %sel = tail call i32 @llvm.wasm.get.ehselector(token %cp)
  %dom = call ptr @__cxa_error_domain_itanium_exception_ptr() [ "funclet"(token %cp) ]
  %code = call i32 @__cxa_error_code_itanium_exception_ptr(ptr noundef %exn) [ "funclet"(token %cp) ]
  catchret from %cp to label %cont

ctor.cleanup:
  %clp = cleanuppad within none []
  call void @__cxa_free_exception(ptr nonnull %obj) [ "funclet"(token %clp) ]
  cleanupret from %clp unwind label %cs

cont:
  %e0 = insertvalue { ptr, i32 } poison, ptr %dom, 0
  %e1 = insertvalue { ptr, i32 } %e0, i32 %code, 1
  %r0 = insertvalue { { ptr, i32 }, i1 } poison, { ptr, i32 } %e1, 0
  %r1 = insertvalue { { ptr, i32 }, i1 } %r0, i1 true, 1
  ret { { ptr, i32 }, i1 } %r1
}

declare ptr @__gxx_wasm_personality_v0(...)
declare ptr @__cxa_allocate_exception(i32)
declare void @__cxa_throw(ptr, ptr, ptr)
declare void @__cxa_free_exception(ptr)
declare ptr @_ZNSt13runtime_errorC1EPKc(ptr, ptr)
declare void @_ZNSt13runtime_errorD1Ev(ptr)
declare ptr @llvm.wasm.get.exception(token)
declare i32 @llvm.wasm.get.ehselector(token)
declare ptr @__cxa_error_domain_itanium_exception_ptr()
declare i32 @__cxa_error_code_itanium_exception_ptr(ptr)
