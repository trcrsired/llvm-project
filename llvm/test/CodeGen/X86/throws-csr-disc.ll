; RUN: llc -O2 -mtriple=x86_64-linux-gnu < %s | FileCheck %s

; The discriminant is materialized onto EFLAGS.CF by the "addb $-1" glued to
; the return. It must run before the epilogue's callee-saved restores: the
; operand register may itself be callee-saved under register pressure, and
; reading it after the pops would observe the caller's restored value.
; The pops do not write EFLAGS, so the flag survives to the ret.

declare { i64, i1 } @callee(i64)

define { i64, i1 } @f(i64 %x, i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %g, i64 %h, i64 %i, i64 %j) throws {
; CHECK-LABEL: f:
; CHECK:         callq callee
; The flag write precedes the callee-saved restores.
; CHECK:         addb $-1
; CHECK:         popq
; CHECK:         retq
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
