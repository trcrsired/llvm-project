; RUN: llc -mtriple=armv7-unknown-linux-gnueabihf -O2 < %s | FileCheck %s
; RUN: llc -mtriple=thumbv7-unknown-linux-gnueabihf -O2 < %s | FileCheck %s --check-prefix=CHECK-THUMB

; Herbceptions (throws): a callee returns its failure discriminant in CPSR.C, and
; clang materialises it right after the call with
;
;   mov r, #0
;   adc  r, r, #0        ; 0 + 0 + C, i.e. the discriminant
;   cmp  r, {0,1}
;
; When that feeds a branch or a predicated select, nothing needs the
; intermediate value to survive, so the materialisation and its compare fold
; onto the carry the call left behind:
;
;   bl callee
;   b<cc> ... / mov<cc> ...
;
; ARM's call-frame adjustment cannot clobber the carry (it is emitted with the
; non-flag-setting ADD/SUB forms), so this holds however the frame comes out.

declare { i32, i1 } @callee(i32) #0
declare void @capture(i32)

; C set means the callee reported failure, so the true arm (100) is selected on
; the carry being set, and the materialisation disappears.
define i32 @select_on_disc(i32 %x) #1 {
; CHECK-LABEL: select_on_disc:
; CHECK:         bl callee
; CHECK-NEXT:    movwhs r0, #100
; CHECK-NOT:     adc
; CHECK-THUMB-LABEL: select_on_disc:
; CHECK-THUMB:       bl callee
; CHECK-THUMB-NEXT:  it hs
; CHECK-THUMB-NEXT:  movhs r0, #100
  %c = call { i32, i1 } @callee(i32 %x) #0
  %v = extractvalue { i32, i1 } %c, 0
  %d = extractvalue { i32, i1 } %c, 1
  %s = select i1 %d, i32 100, i32 %v
  ret i32 %s
}

; The same fold with the select arms swapped, which flips the condition: the
; callee's value is the true arm, so 100 is selected when C is clear.
define i32 @select_inverted(i32 %x) #1 {
; CHECK-LABEL: select_inverted:
; CHECK:         bl callee
; CHECK-NEXT:    movlo r0, #100
; CHECK-NOT:     adc
; CHECK-THUMB-LABEL: select_inverted:
; CHECK-THUMB:       bl callee
; CHECK-THUMB-NEXT:  it lo
; CHECK-THUMB-NEXT:  movlo r0, #100
  %c = call { i32, i1 } @callee(i32 %x) #0
  %v = extractvalue { i32, i1 } %c, 0
  %d = extractvalue { i32, i1 } %c, 1
  %s = select i1 %d, i32 %v, i32 100
  ret i32 %s
}

; A flag-setting instruction between the materialisation and its compare
; redefines CPSR, so the consumer is no longer reading the discriminant and the
; fold must not fire: dropping the materialisation would leave the consumer
; reading this cmp. The whole sequence has to survive.
define i32 @select_clobber(i32 %x, i32 %y) #1 {
; CHECK-LABEL: select_clobber:
; CHECK:         bl callee
; CHECK:         adc r1, r1, #0
; CHECK:         cmp r0, r4
; CHECK:         cmp r1, #0
; CHECK:         movwne r0, #100
  %c = call { i32, i1 } @callee(i32 %x) #0
  %v = extractvalue { i32, i1 } %c, 0
  %d = extractvalue { i32, i1 } %c, 1
  %cmp = icmp sgt i32 %v, %y
  %sv = select i1 %cmp, i32 %v, i32 %y
  %s = select i1 %d, i32 100, i32 %sv
  ret i32 %s
}

; Two consumers reading the same discriminant share the flags the compare sets,
; and folding removes that compare, so the fold must not fire: rewriting only
; one consumer would leave the other testing whatever CPSR the call left
; behind. The materialisation stays and both consumers keep reading it.
define i32 @two_readers(i32 %x) #1 {
; CHECK-LABEL: two_readers:
; CHECK:         bl callee
; CHECK-NEXT:    mov r1, #0
; CHECK-NEXT:    adcs r1, r1, #0
; CHECK-NEXT:    movwne r0, #100
; CHECK-NEXT:    movwne r0, #200
; CHECK-THUMB-LABEL: two_readers:
; CHECK-THUMB:       bl callee
; CHECK-THUMB-NEXT:  mov.w r1, #0
; CHECK-THUMB-NEXT:  adcs r1, r1, #0
; CHECK-THUMB-NEXT:  itt ne
; CHECK-THUMB-NEXT:  movne r0, #100
; CHECK-THUMB-NEXT:  movne r0, #200
  %c = call { i32, i1 } @callee(i32 %x) #0
  %v = extractvalue { i32, i1 } %c, 0
  %d = extractvalue { i32, i1 } %c, 1
  %a = select i1 %d, i32 100, i32 %v
  %b = select i1 %d, i32 200, i32 %a
  ret i32 %b
}

; Branching on the discriminant folds too. The error path is taken on C set, so
; the return out of the success path is predicated on C clear, and asking the
; other way round predicates it on C set instead - note that clang emits the
; compare against one there, not against zero.
define void @branch_on_disc(i32 %x) #1 {
; CHECK-LABEL: branch_on_disc:
; CHECK:         bl callee
; CHECK-NEXT:    poplo
; CHECK-NOT:     adc
; CHECK-THUMB-LABEL: branch_on_disc:
; CHECK-THUMB:       bl callee
; CHECK-THUMB-NEXT:  itt hs
  %c = call { i32, i1 } @callee(i32 %x) #0
  %d = extractvalue { i32, i1 } %c, 1
  br i1 %d, label %err, label %ok
err:
  call void @capture(i32 1)
  ret void
ok:
  ret void
}

define void @branch_inverted(i32 %x) #1 {
; CHECK-LABEL: branch_inverted:
; CHECK:         bl callee
; CHECK-NEXT:    pophs
; CHECK-NOT:     adc
; CHECK-THUMB-LABEL: branch_inverted:
; CHECK-THUMB:       bl callee
; CHECK-THUMB-NEXT:  itt lo
  %c = call { i32, i1 } @callee(i32 %x) #0
  %d = extractvalue { i32, i1 } %c, 1
  br i1 %d, label %ok, label %err
err:
  call void @capture(i32 1)
  ret void
ok:
  ret void
}

attributes #0 = { throws }
attributes #1 = { nounwind }
