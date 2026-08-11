; RUN: llc -mtriple=x86_64-unknown-linux-gnu < %s | FileCheck %s
; RUN: llc -mtriple=i686-unknown-linux-gnu < %s | FileCheck %s --check-prefix=CHECK32

; A function with the throws (herbception) attribute returns its value with a
; discriminant. On x86-64/x86-32 the discriminant is carried in the carry flag
; (CF) instead of a return register.

; Success: discriminant is false (0) -> CF is clear.
define { i64, i1 } @ret_success(i64 %x) #0 {
; CHECK-LABEL: ret_success:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movq %rdi, %rax
; CHECK-NEXT:    xorl %ecx, %ecx
; CHECK-NEXT:    addb $-1, %cl
; CHECK-NEXT:    retq
entry:
  %r.i = insertvalue { i64, i1 } poison, i64 %x, 0
  %r = insertvalue { i64, i1 } %r.i, i1 false, 1
  ret { i64, i1 } %r
}

; Error: discriminant is true (1) -> CF is set.
define { i64, i1 } @ret_error(i64 %x) #0 {
; CHECK-LABEL: ret_error:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movq %rdi, %rax
; CHECK-NEXT:    movb $1, %cl
; CHECK-NEXT:    addb $-1, %cl
; CHECK-NEXT:    retq
entry:
  %r.i = insertvalue { i64, i1 } poison, i64 %x, 0
  %r = insertvalue { i64, i1 } %r.i, i1 true, 1
  ret { i64, i1 } %r
}

; The caller reads the discriminant from CF right after the call (setb).
define i64 @call_and_select(i64 %x) #1 {
; CHECK-LABEL: call_and_select:
; CHECK:       # %bb.0:
; CHECK-NEXT:    pushq %rax
; CHECK-NEXT:    callq ret_error@PLT
; CHECK-NEXT:    setb %cl
; CHECK-NEXT:    testb $1, %cl
; CHECK-NEXT:    movl $100, %ecx
; CHECK-NEXT:    cmovneq %rcx, %rax
; CHECK-NEXT:    popq %rcx
; CHECK-NEXT:    retq
; CHECK32-LABEL: call_and_select:
; CHECK32:       # %bb.0:
; CHECK32:         calll ret_error@PLT
; CHECK32-NEXT:    setb %cl
; CHECK32-NEXT:    testb $1, %cl
entry:
  %c = call { i64, i1 } @ret_error(i64 %x)
  %val = extractvalue { i64, i1 } %c, 0
  %disc = extractvalue { i64, i1 } %c, 1
  %sel = select i1 %disc, i64 100, i64 %val
  ret i64 %sel
}

attributes #0 = { throws }
attributes #1 = { nounwind }
