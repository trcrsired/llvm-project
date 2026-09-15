; The trailing i1 of a `throws` (herbception) function's {T, i1} return is
; the failure discriminant delivered through the carry flag. It is part of
; the calling convention and must stay even when no caller inspects it;
; otherwise the backend would treat the real last payload element as the
; discriminant.
;
; RUN: opt -passes=deadargelim -S < %s | FileCheck %s

; CHECK-LABEL: define internal { i32, i1 } @throws_fn()
define internal { i32, i1 } @throws_fn() #0 {
entry:
  ret { i32, i1 } { i32 7, i1 true }
}

; The caller only reads element 0; the i1 discriminant is unused but must
; not be stripped.
; CHECK-LABEL: define i32 @caller()
; CHECK: %call = call { i32, i1 } @throws_fn()
define i32 @caller() {
entry:
  %call = call { i32, i1 } @throws_fn()
  %v = extractvalue { i32, i1 } %call, 0
  ret i32 %v
}

attributes #0 = { throws }
