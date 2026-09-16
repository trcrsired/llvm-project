; RUN: opt -passes=herbceptions-legacy-eh-fold -S < %s | FileCheck %s
;
; Wasm EH model (catchswitch/catchpad/catchret like MSVC, but the
; conversion uses the Itanium entry points on the wasm.get.exception
; value). Real CodeGen emits the conversion calls as invokes carrying
; "funclet" bundles and spreads them across several blocks; the chain
; ends in catchret. The ctor-failure cleanup edge shares the
; catchswitch and must be preserved.

target triple = "wasm32-unknown-wasip1"

@_ZTISt13runtime_error = external constant ptr

; CHECK-LABEL: define {{.*}} @foo
define hidden { { ptr, i32 }, i1 } @foo() local_unnamed_addr personality ptr @__gxx_wasm_personality_v0 {
entry:
  %obj = tail call ptr @__cxa_allocate_exception(i32 8)
  invoke ptr @_ZNSt13runtime_errorC1EPKc(ptr noundef nonnull align 4 dereferenceable(8) %obj, ptr noundef nonnull null)
          to label %ctor.ok unwind label %ctor.cleanup

ctor.ok:
; The folded path keeps the domain helper's unwind edge (the exception
; could still escape if the domain call itself throws) but replaces the
; legacy code conversion with the direct helper in a new block.
; CHECK-NOT: __cxa_throw
; CHECK: %[[DOM:.*]] = invoke ptr @__cxa_error_domain_itanium_exception_ptr()
; CHECK-NEXT: to label %[[DOMOK:.*]] unwind label %[[UNW:.*]]
  invoke void @__cxa_throw(ptr nonnull %obj, ptr nonnull @_ZTISt13runtime_error, ptr nonnull @_ZNSt13runtime_errorD1Ev)
          to label %dead unwind label %cs

dead:
  unreachable

; The catchswitch stays alive for the ctor-failure edge and keeps its
; real catchpad conversion chain.
; CHECK: catchswitch
; CHECK: catchpad
; CHECK: invoke ptr @__cxa_error_domain_itanium_exception_ptr() [ "funclet"
; CHECK: invoke i32 @__cxa_error_code_itanium_exception_ptr(
; CHECK: catchret
; CHECK: cont:
; CHECK: phi ptr {{.*}}%[[DOM]]
; CHECK: phi i32 {{.*}}%herb.code
;
; The new folded block is appended at the end of the function.
; CHECK: [[DOMOK]]:
; CHECK: %herb.code = call i32 @__cxa_error_code_itanium_exception_ptr_direct(ptr %obj, ptr @_ZTISt13runtime_error, ptr @_ZNSt13runtime_errorD1Ev)
; CHECK: br label %cont
cs:
  %cswtok = catchswitch within none [label %pad] unwind label %unwind

pad:
  %cp = catchpad within %cswtok [ptr null]
  %exn = call ptr @llvm.wasm.get.exception(token %cp)
  %sel = call i32 @llvm.wasm.get.ehselector(token %cp)
  %dom = invoke ptr @__cxa_error_domain_itanium_exception_ptr() [ "funclet"(token %cp) ]
          to label %code.bb unwind label %unwind

code.bb:
  %code = invoke i32 @__cxa_error_code_itanium_exception_ptr(ptr noundef %exn) [ "funclet"(token %cp) ]
          to label %ret.bb unwind label %unwind

ret.bb:
  catchret from %cp to label %cont

ctor.cleanup:
  %clp = cleanuppad within none []
  call void @__cxa_free_exception(ptr nonnull %obj) [ "funclet"(token %clp) ]
  cleanupret from %clp unwind label %cs

unwind:
  %ulp = cleanuppad within none []
  cleanupret from %ulp unwind to caller

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
