; RUN: llc -mtriple=sparc-unknown-linux-gnu < %s | FileCheck %s --check-prefix=V8
; RUN: llc -mtriple=sparcv9-unknown-linux-gnu < %s | FileCheck %s --check-prefix=V9

; A function with the throws (herbception) attribute returns its value with a
; discriminant. On SPARC the discriminant is carried in the integer condition
; code carry bit: %icc.c (v8; v9 also sees it via %xcc.c, which subcc writes
; simultaneously). The callee emits a cmp glued before retl; the caller reads
; the bit back with movcs/select-icc or branches on it directly.

; Success: discriminant is false (0) -> carry clear.
define { i64, i1 } @ret_success(i64 %x) #0 {
; V8-LABEL: ret_success:
; V8:            retl
; V8-NEXT:       cmp %g0, 0
; V9-LABEL: ret_success:
; V9:            retl
; V9-NEXT:       cmp %g0, 0
entry:
  %r.i = insertvalue { i64, i1 } poison, i64 %x, 0
  %r = insertvalue { i64, i1 } %r.i, i1 false, 1
  ret { i64, i1 } %r
}

; Error: discriminant is true (1) -> carry set.
define { i64, i1 } @ret_error(i64 %x) #0 {
; V8-LABEL: ret_error:
; V8:            retl
; V8-NEXT:       cmp %g0, 1
; V9-LABEL: ret_error:
; V9:            retl
; V9-NEXT:       cmp %g0, 1
entry:
  %r.i = insertvalue { i64, i1 } poison, i64 %x, 0
  %r = insertvalue { i64, i1 } %r.i, i1 true, 1
  ret { i64, i1 } %r
}

; The caller folds a branch on the discriminant into a conditional branch on
; the condition code, immediately after the call: carry set = error.
define i64 @call_and_branch(i64 %x) #1 {
; V8-LABEL: call_and_branch:
; V8:            call ret_error
; V8:            bcs
; V9-LABEL: call_and_branch:
; V9:            call ret_error
; V9-NEXT:       mov %i0, %o0
; V9-NEXT:       bcc
entry:
  %c = call { i64, i1 } @ret_error(i64 %x)
  %val = extractvalue { i64, i1 } %c, 0
  %disc = extractvalue { i64, i1 } %c, 1
  br i1 %disc, label %err, label %ok
err:
  ret i64 -1
ok:
  ret i64 %val
}

; The caller reads the discriminant back out of the carry bit with a select.
define i64 @call_and_select(i64 %x) #1 {
; V9-LABEL: call_and_select:
; V9:            call ret_error
; V9:            movcs %icc, 1, %i0
entry:
  %c = call { i64, i1 } @ret_error(i64 %x)
  %val = extractvalue { i64, i1 } %c, 0
  %disc = extractvalue { i64, i1 } %c, 1
  %sel = select i1 %disc, i64 100, i64 %val
  ret i64 %sel
}

; throws_sret: the payload travels through the hidden pointer; the register
; return carries {E, i1} -- the error value in %o0:%o1 plus the discriminant
; in the carry bit.
define { i64, i64, i1 } @ret_sret64(ptr throws_sret({i64,i64,i64}) %out, i64 %x) #0 {
; V9-LABEL: ret_sret64:
; V9:            stx %o1, [%o0]
; V9:            mov 42, %o1
; V9:            cmp %g0, 1
; V9:            retl
entry:
  store i64 %x, ptr %out, align 8
  %r.i = insertvalue { i64, i64, i1 } poison, i64 %x, 0
  %r.i2 = insertvalue { i64, i64, i1 } %r.i, i64 42, 1
  %r = insertvalue { i64, i64, i1 } %r.i2, i1 true, 2
  ret { i64, i64, i1 } %r
}

attributes #0 = { throws }
attributes #1 = { nounwind }
