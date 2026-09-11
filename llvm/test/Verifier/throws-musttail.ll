; RUN: not llvm-as %s -o /dev/null 2>&1 | FileCheck %s

; A musttail call has to preserve the callee's return convention for the
; caller's own caller. Whether a function uses the herbception (throws)
; convention decides where the return discriminant lives -- in the carry flag,
; or in an ordinary return register -- so the caller and callee must agree on
; it. 'throws' is a function attribute, not a parameter attribute, so the
; existing ABI-attribute comparison does not catch a mismatch: a 'throws'
; function that tail-called a plain function of the same signature would return
; a discriminant its caller reads from a flag the callee never set.

declare { i64, i1 } @plain(i32)

; CHECK: cannot guarantee tail call due to mismatched 'throws' attributes
define { i64, i1 } @throws_tails_non_throws(i32 %n) #0 {
  %r = musttail call { i64, i1 } @plain(i32 %n)
  ret { i64, i1 } %r
}

; Matching conventions are still allowed in either direction.
define { i64, i1 } @throws_tails_throws(i32 %n) #0 {
  %r = musttail call { i64, i1 } @throws_callee(i32 %n) #0
  ret { i64, i1 } %r
}

define { i64, i1 } @throws_callee(i32 %n) #0 {
  %r = insertvalue { i64, i1 } poison, i64 0, 0
  %r2 = insertvalue { i64, i1 } %r, i1 false, 1
  ret { i64, i1 } %r2
}

define { i64, i1 } @plain_tails_plain(i32 %n) {
  %r = musttail call { i64, i1 } @plain(i32 %n)
  ret { i64, i1 } %r
}

attributes #0 = { throws }
