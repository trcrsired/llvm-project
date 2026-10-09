; RUN: llc -mtriple=aarch64-unknown-linux-gnu -O0 %s -o - | FileCheck %s

; Herbception (throws): same contract as
; GlobalISel/herbceptions-throws-call-discriminant.ll but on the -O0
; FastISel path (the default instruction selector for eligible calls at
; -O0 outside of GlobalISel builds): the call's discriminant comes from
; NZCV.C (cset .., hs), not from the leaf's AAPCS-assigned register (w2).

declare { { ptr, i64 }, i1 } @throws_callee(i32) #0

define i32 @caller(i32 %x) {
; CHECK-LABEL: caller:
; CHECK:         bl throws_callee
; CHECK-NOT:     w2
; CHECK:         cset {{w[0-9]+}}, hs
  %r = call { { ptr, i64 }, i1 } @throws_callee(i32 %x) #1
  %d = extractvalue { { ptr, i64 }, i1 } %r, 1
  %s = select i1 %d, i32 -1, i32 0
  ret i32 %s
}

attributes #0 = { throws }
attributes #1 = { nounwind throws }
