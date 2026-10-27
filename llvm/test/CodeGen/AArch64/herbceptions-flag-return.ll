; RUN: llc -mtriple=aarch64-unknown-linux-gnu < %s | FileCheck %s

; The herbceptions-flag-return pass duplicates a small return block of a
; 'throws' function into its predecessors and turns the discriminant into a
; constant where the branch into the return implies it. On AArch64 a constant
; discriminant is returned by a return sequence that sets NZCV.C itself
; (cmp wzr, #1 / cmp wzr, #0), so the flag is never materialized in W16, and
; the two constant returns stay distinct so tail merging cannot fold them into
; a phi in a callee-saved register.
;
; The arms store to memory so the return diamond cannot be folded into a
; select before the pass runs; the merged return block is what the pass works
; on.

; A phi whose incoming values are constants.
define { i64, i1 } @dup_constants(i64 %v, ptr %p1, ptr %p2, i1 %take_error) #0 {
; CHECK-LABEL: dup_constants:
; CHECK-NOT:     mov w16
; CHECK-DAG:     subs wzr, wzr, #1
; CHECK-DAG:     subs wzr, wzr, #0
; CHECK:         ret
entry:
  br i1 %take_error, label %error, label %success

error:
  store volatile i64 1, ptr %p1
  br label %return

success:
  store volatile i64 2, ptr %p2
  br label %return

return:
  %flag = phi i1 [ true, %error ], [ false, %success ]
  %ret.0 = insertvalue { i64, i1 } poison, i64 %v, 0
  %ret.1 = insertvalue { i64, i1 } %ret.0, i1 %flag, 1
  ret { i64, i1 } %ret.1
}

; The discriminant is the branch condition itself, so the branch implies it on
; both edges.
define { i64, i1 } @implied_by_branch(i64 %v, ptr %p1, ptr %p2, i32 %e) #0 {
; CHECK-LABEL: implied_by_branch:
; CHECK-NOT:     mov w16
; CHECK-DAG:     subs wzr, wzr, #1
; CHECK-DAG:     subs wzr, wzr, #0
; CHECK:         ret
entry:
  %bad = icmp ne i32 %e, 0
  br i1 %bad, label %error, label %success

error:
  store volatile i64 1, ptr %p1
  br label %return

success:
  store volatile i64 2, ptr %p2
  br label %return

return:
  %flag = phi i1 [ %bad, %error ], [ %bad, %success ]
  %ret.0 = insertvalue { i64, i1 } poison, i64 %v, 0
  %ret.1 = insertvalue { i64, i1 } %ret.0, i1 %flag, 1
  ret { i64, i1 } %ret.1
}

attributes #0 = { throws }
