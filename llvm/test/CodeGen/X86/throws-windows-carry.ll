; RUN: llc -mtriple=x86_64-unknown-windows-msvc < %s | FileCheck %s
; RUN: llc -mtriple=i686-unknown-windows-msvc < %s | FileCheck %s --check-prefix=CHECK32
; RUN: llc -mtriple=aarch64-unknown-windows-msvc < %s | FileCheck %s --check-prefix=CHECK64

; Herbception (throws): the callee returns the discriminant in CF and clang
; materializes it with HERB_SETCCr, so the branch initially tests the
; materialized register. The fold that replaces that test with a direct branch
; on live CF runs after prolog/epilog insertion, which is the point at which it
; is finally known whether the call's stack adjustment survives as a real
; add/sub -- such an add redefines CF, and on i686 it used to be skipped, so the
; branch ended up testing the stack cleanup's carry-out and the error path
; became unreachable. By the time the fold runs, that adjustment has either
; been merged away (as here, on both x86 targets) or is visible as a real
; instruction that disqualifies the fold.
;
; Branch-only discriminant use folds to a single jcc on CF.
declare void @capture(i32)
declare { i32, i1 } @foo(i32) #0

define void @call_and_branch(i32 %x) #1 {
; CHECK-LABEL: call_and_branch:
; CHECK:       callq foo
; CHECK-NEXT:  j{{b|ae}} .LBB0_
; CHECK32-LABEL: _call_and_branch:
; CHECK32:       calll _foo
; CHECK32-NEXT:  j{{b|ae}} LBB0_
; CHECK64-LABEL: call_and_branch:
; CHECK64:       bl foo
; CHECK64-NEXT:  b.lo .LBB0_
entry:
  %c = call { i32, i1 } @foo(i32 %x) #1
  %d = extractvalue { i32, i1 } %c, 1
  br i1 %d, label %err, label %cont
err:
  call void @capture(i32 7)
  ret void
cont:
  ret void
}

; Callee side: throws function returns discriminant in CF.
; Success path: clc + ret.
define { i32, i1 } @ret_success(i32 %x) #0 {
; CHECK-LABEL: ret_success:
; CHECK:       movl %ecx, %eax
; CHECK-NEXT:  clc
; CHECK:       retq
; CHECK32-LABEL: _ret_success:
; CHECK32:       movl 4(%esp), %eax
; CHECK32-NEXT:  clc
; CHECK32:       retl
; CHECK64-LABEL: ret_success:
; CHECK64:       mov w8, wzr
; CHECK64-NEXT:  cmp w8, #1
; CHECK64:       ret
entry:
  %r = insertvalue { i32, i1 } poison, i32 %x, 0
  %r1 = insertvalue { i32, i1 } %r, i1 false, 1
  ret { i32, i1 } %r1
}

; Error path: stc + ret.
define { i32, i1 } @ret_error(i32 %x) #0 {
; CHECK-LABEL: ret_error:
; CHECK:       movl %ecx, %eax
; CHECK-NEXT:  stc
; CHECK:       retq
; CHECK32-LABEL: _ret_error:
; CHECK32:       movl 4(%esp), %eax
; CHECK32-NEXT:  stc
; CHECK32:       retl
; CHECK64-LABEL: ret_error:
; CHECK64:       mov w8, #1
; CHECK64-NEXT:  cmp w8, #1
; CHECK64:       ret
entry:
  %r = insertvalue { i32, i1 } poison, i32 %x, 0
  %r1 = insertvalue { i32, i1 } %r, i1 true, 1
  ret { i32, i1 } %r1
}

; CMOV discriminant use: x86_64 folds the test away and feeds the cmov directly
; from CF. i686 lowers the select to a branch instead and puts a zero-idiom
; between the setb and the test; that zero-idiom redefines the flags, so the
; branch has to keep testing the materialized register.
define i64 @call_and_select(i64 %x) #1 {
; CHECK-LABEL: call_and_select:
; CHECK:       callq foo
; CHECK-NOT:   setb
; CHECK-NOT:   testb
; CHECK:       cmov{{b|ae}}
; CHECK32-LABEL: _call_and_select:
; CHECK32:       calll _foo
; CHECK32:       setb %{{[a-z0-9]+}}
; CHECK32:       testb $1, %{{[a-z0-9]+}}
; CHECK32:       j{{e|ne}} LBB3_
entry:
  %c = call { i64, i1 } @foo(i64 %x) #1
  %val = extractvalue { i64, i1 } %c, 0
  %disc = extractvalue { i64, i1 } %c, 1
  %sel = select i1 %disc, i64 100, i64 %val
  ret i64 %sel
}

attributes #0 = { throws }
attributes #1 = { throws }
