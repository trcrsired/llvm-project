; RUN: llc -mtriple=sparcv9-unknown-linux-gnu < %s | FileCheck %s

; The stack protector lowering splices an MBB's tail at its terminator
; sequence and emits the cookie check right before it. The subcc that
; materializes the discriminant onto ICC.C is not a copy so it is not part of
; that sequence; without including it, the cookie compare becomes the last
; flag write before the return and the function would report the cookie
; result instead of its own discriminant. The flag write must travel with the
; terminator into the success block, and the return must model its implicit
; ICC use so the flag producer is kept live.

declare { { ptr, i64 }, i1 } @callee() #0

define { { ptr, i64 }, i1 } @f() #1 {
; CHECK-LABEL: {{^"?#?f"?}}:
; CHECK:      call {{"?#?}}callee{{"?}}
; CHECK:      cmp %i{{[0-9]+}}, %i{{[0-9]+}}
; CHECK-NEXT: bne %xcc
; CHECK:      cmp %g0, %i{{[0-9]+}}
; CHECK-NEXT: ret
; CHECK-NEXT: restore
entry:
  %a = alloca [32 x i8], align 1
  call void @llvm.lifetime.start.p0(ptr nonnull %a) #3
  %c = call { { ptr, i64 }, i1 } @callee() #2
  %d = extractvalue { { ptr, i64 }, i1 } %c, 1
  br i1 %d, label %common, label %callblk
callblk:
  store i8 1, ptr %a, align 1
  call void asm sideeffect "", "r,~{memory}"(ptr nonnull %a) #3
  br label %common
common:
  call void @llvm.lifetime.end.p0(ptr nonnull %a) #3
  ret { { ptr, i64 }, i1 } %c
}

declare void @llvm.lifetime.start.p0(ptr captures(none))
declare void @llvm.lifetime.end.p0(ptr captures(none))

attributes #0 = { nounwind throws }
attributes #1 = { nounwind sspstrong throws "stack-protector-buffer-size"="8" }
attributes #2 = { nounwind throws }
attributes #3 = { nounwind }
