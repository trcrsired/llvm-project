; A 'throws' (herbceptions) coroutine still gets musttail symmetric-transfer
; calls in its split functions. The split functions use the plain
; resume-function ABI (void(ptr)) and must not inherit 'throws': their
; musttail calls carry no 'throws' call-site attribute, so a 'throws' caller
; would trip the musttail verifier check.
; RUN: opt < %s -passes='cgscc(coro-split),simplifycfg,early-cse,verify' -S | FileCheck %s

define void @f() #0 {
entry:
  %id = call token @llvm.coro.id(i32 0, ptr null, ptr @f, ptr null)
  %alloc = call ptr @malloc(i64 16)
  %vFrame = call noalias nonnull ptr @llvm.coro.begin(token %id, ptr %alloc)

  %save = call token @llvm.coro.save(ptr null)
  %addr1 = call ptr @llvm.coro.subfn.addr(ptr null, i8 0)
  call void %addr1(ptr null)

  %suspend = call i8 @llvm.coro.suspend(token %save, i1 false)
  switch i8 %suspend, label %exit [
    i8 0, label %await.ready
    i8 1, label %exit
  ]
await.ready:
  %save2 = call token @llvm.coro.save(ptr null)
  call void @llvm.coro.await.suspend.handle(ptr null, ptr null, ptr @await_suspend_function)

  %suspend2 = call i8 @llvm.coro.suspend(token %save2, i1 false)
  switch i8 %suspend2, label %exit [
    i8 0, label %exit
    i8 1, label %exit
  ]
exit:
  call void @llvm.coro.end(ptr null, i1 false, token none)
  ret void
}

; The ramp keeps 'throws'.
; CHECK-LABEL: define void @f(
; CHECK-SAME: #[[RAMPATTR:[0-9]+]]

; The split functions are plain resume functions: no attribute group, and
; their symmetric-transfer resume calls are musttail.
; CHECK-LABEL: define internal void @f.resume(
; CHECK-NOT: #
; CHECK: musttail call void
; CHECK-LABEL: define internal void @f.destroy(
; CHECK-NOT: #
; CHECK: musttail call void
; CHECK-LABEL: define internal void @f.cleanup(
; CHECK-NOT: #
; CHECK: musttail call void

; CHECK: attributes #[[RAMPATTR]] = { throws }

declare token @llvm.coro.id(i32, ptr readnone, ptr nocapture readonly, ptr) #1
declare i1 @llvm.coro.alloc(token) #2
declare i64 @llvm.coro.size.i64() #3
declare ptr @llvm.coro.begin(token, ptr writeonly) #2
declare token @llvm.coro.save(ptr) #2
declare ptr @llvm.coro.frame() #3
declare i8 @llvm.coro.suspend(token, i1) #2
declare ptr @llvm.coro.free(token, ptr nocapture readonly) #1
declare void @llvm.coro.end(ptr, i1, token) #2
declare ptr @llvm.coro.subfn.addr(ptr nocapture readonly, i8) #1
declare ptr @malloc(i64)
declare ptr @await_suspend_function(ptr %awaiter, ptr %hdl)

attributes #0 = { presplitcoroutine throws }
attributes #1 = { argmemonly nounwind readonly }
attributes #2 = { nounwind }
attributes #3 = { nounwind readnone }
