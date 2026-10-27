; RUN: llc -mtriple=x86_64-unknown-linux-gnu < %s | FileCheck %s
; RUN: llc -mtriple=i686-unknown-linux-gnu < %s | FileCheck %s --check-prefix=CHECK32

; The herbceptions-flag-return pass duplicates a small return block of a
; 'throws' function into its predecessors. When the discriminant is a phi, the
; copy of the return in each predecessor sees the incoming value directly; when
; that value is implied by the branch into the return, the pass uses a
; constant. LowerReturn then sets the carry flag with stc/clc instead of
; materializing the discriminant in a register.
;
; The arms store to memory so the return diamond cannot be folded into a
; select before the pass runs; the merged return block is what the pass works
; on.

; A phi whose incoming values are constants: each duplicated return gets its
; own constant and no setb remains.
define { i64, i1 } @dup_constants(i64 %v, ptr %p1, ptr %p2, i1 %take_error) #0 {
; CHECK-LABEL: dup_constants:
; CHECK-NOT:     setb
; CHECK-DAG:     stc
; CHECK-DAG:     clc
; CHECK32-LABEL: dup_constants:
; CHECK32-NOT:   setb
; CHECK32-DAG:   stc
; CHECK32-DAG:   clc
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

; The discriminant is the branch condition itself. On the error edge it is
; true, on the success edge the branch implies it is false, so both duplicated
; returns use a constant as well.
define { i64, i1 } @implied_by_branch(i64 %v, ptr %p1, ptr %p2, i32 %e) #0 {
; CHECK-LABEL: implied_by_branch:
; CHECK-NOT:     setb
; CHECK-DAG:     stc
; CHECK-DAG:     clc
; CHECK32-LABEL: implied_by_branch:
; CHECK32-NOT:   setb
; CHECK32-DAG:   stc
; CHECK32-DAG:   clc
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
