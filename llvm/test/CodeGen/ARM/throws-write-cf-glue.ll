; RUN: llc -mtriple=armv7-unknown-linux-gnueabihf < %s | FileCheck %s

; The stack protector lowering splices an MBB's tail at its terminator
; sequence and emits the cookie check right before it. The instruction that
; materializes the discriminant onto CPSR.C is not a copy so it is not part of
; that sequence; without including it, the cookie compare becomes the last
; flag write before the return and the function would report the cookie
; result instead of its own discriminant. The flag write must travel with the
; terminator into the success block.

declare { { ptr, i32 }, i1 } @callee() #0

define { { ptr, i32 }, i1 } @f() #1 {
; CHECK-LABEL: {{^"?#?f"?}}:
; CHECK:      bl {{"?#?}}callee{{"?}}
; CHECK:      cmp
; CHECK-NEXT: bne
; CHECK:      subs {{.*}}, {{.*}}, #1
; CHECK-NEXT: add{{.*}}sp, sp, #40
; CHECK-NEXT: pop{{.*}}{r11, pc}
entry:
  %a = alloca [32 x i8], align 1
  call void @llvm.lifetime.start.p0(ptr nonnull %a) #3
  %c = call { { ptr, i32 }, i1 } @callee() #2
  %d = extractvalue { { ptr, i32 }, i1 } %c, 1
  br i1 %d, label %common, label %callblk
callblk:
  store i8 1, ptr %a, align 1
  call void asm sideeffect "", "r,~{memory}"(ptr nonnull %a) #3
  br label %common
common:
  call void @llvm.lifetime.end.p0(ptr nonnull %a) #3
  ret { { ptr, i32 }, i1 } %c
}

declare void @llvm.lifetime.start.p0(ptr captures(none))
declare void @llvm.lifetime.end.p0(ptr captures(none))

attributes #0 = { nounwind throws }
attributes #1 = { nounwind sspstrong throws "stack-protector-buffer-size"="8" }
attributes #2 = { nounwind throws }
attributes #3 = { nounwind }
