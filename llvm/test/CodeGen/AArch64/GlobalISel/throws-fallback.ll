; RUN: llc -O0 -global-isel -global-isel-abort=2 -pass-remarks-missed='gisel*' \
; RUN:     -mtriple=aarch64 < %s -o %t.out 2> %t.err
; RUN: FileCheck %s --check-prefix=ERR < %t.err
; RUN: FileCheck %s --check-prefix=ASM < %t.out

; A herbception (throws) function returns its discriminant in the carry flag,
; which makes the flag part of the calling convention. GlobalISel does not
; implement that: it would lower the return as an ordinary {payload, i1}
; aggregate and put the discriminant in a register, quietly giving the function
; an ABI that disagrees with SelectionDAG's. The function must fall back
; instead, so that -O0 and -O2 agree.

; The fallback has to be driven by the 'throws' attribute, since the return
; type {E, i1} alone is an ordinary aggregate that GlobalISel handles fine.
; ERR: unable to lower function: {{.*}} (in function: good)
; ERR: warning: Instruction selection used fallback path for good
define { i64, i1 } @good(i32 %n) #1 {
  %r = insertvalue { i64, i1 } poison, i64 7, 0
  %r2 = insertvalue { i64, i1 } %r, i1 false, 1
  ret { i64, i1 } %r2
}

; Falling back must produce SelectionDAG's convention: the discriminant is
; compared out of NZCV.C, not copied out of a return register.
; ASM-LABEL: good:
; ASM:       subs w8, w8, #1

; The same signature without 'throws' is unaffected and stays on GlobalISel.
declare { i64, i1 } @plain(i32)
; ASM-LABEL: plain_tail:
; ASM:       b plain
define { i64, i1 } @plain_tail(i32 %n) {
  %r = tail call { i64, i1 } @plain(i32 %n)
  ret { i64, i1 } %r
}

attributes #1 = { throws }
