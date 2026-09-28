; RUN: llc -O2 -mtriple=aarch64 < %s | FileCheck %s

; The NZCV.C materialization inside RET_ReallyLR runs after the epilogue has
; restored every callee-saved register. If the discriminant's value lived in a
; callee-saved register (as happens under register pressure -- the bug this
; tests for had the disc in W21, clobbered by "ldp x22, x21" before the cmp
; read it), the compare would observe the caller's restored value and return a
; bogus "threw" flag.
;
; LowerReturn therefore always routes the discriminant through W16 (IP0):
; the copy is emitted with the return-value copies, which run before the
; restores, and W16 itself is call-clobbered scratch that no epilogue code
; touches. Check the copy lands before the ldp restores and that the flag
; write reads W16, never a callee-saved register.

declare { i64, i1 } @callee(i64)

define { i64, i1 } @f(i64 %x, i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %g, i64 %h, i64 %i, i64 %j) throws {
; CHECK-LABEL: f:
; CHECK:         bl callee
; The discriminant (the i1 result of callee, here in w1) is copied into w16
; among the return-value copies, before the callee-saved restores.
; CHECK:         and w16, w1, #0x1
; CHECK:         ldp
; CHECK:         ldp
; CHECK:         ldp
; CHECK:         ldp
; The flag write is the last instruction before ret and reads w16.
; CHECK:         cmp w16, #1
; CHECK-NEXT:    ret
; CHECK-NOT:     cmp w{{19|2[0-9]}}, #1
entry:
  %r = call { i64, i1 } @callee(i64 %x)
  %v = extractvalue { i64, i1 } %r, 0
  %disc = extractvalue { i64, i1 } %r, 1
  %s1 = add i64 %a, %b
  %s2 = add i64 %c, %d
  %s3 = add i64 %e, %g
  %s4 = add i64 %h, %i
  %s5 = add i64 %j, %v
  store volatile i64 %s1, ptr null
  store volatile i64 %s2, ptr null
  store volatile i64 %s3, ptr null
  store volatile i64 %s4, ptr null
  store volatile i64 %s5, ptr null
  %out = insertvalue { i64, i1 } poison, i64 %v, 0
  %out2 = insertvalue { i64, i1 } %out, i1 %disc, 1
  ret { i64, i1 } %out2
}
