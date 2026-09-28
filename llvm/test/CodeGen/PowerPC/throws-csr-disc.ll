; RUN: llc -O2 -mtriple=ppc64le-linux-gnu < %s | FileCheck %s

; The discriminant is materialized onto CR6 by a cmp/mcrf before the return.
; It must run before the epilogue's callee-saved restores: the operand
; register may itself be callee-saved under register pressure, and reading
; it after the loads would observe the caller's restored value. The ld
; restores do not write CR6, so the flag survives to the blr.

declare { i64, i1 } @callee(i64)

define { i64, i1 } @f(i64 %x, i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %g, i64 %h, i64 %i, i64 %j) throws {
; CHECK-LABEL: f:
; CHECK:         bl callee
; The CR6 write precedes the callee-saved restores.
; CHECK:         mcrf 6
; CHECK:         ld
; CHECK:         blr
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
