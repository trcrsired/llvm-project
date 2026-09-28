; RUN: llc -mtriple=aarch64-unknown-linux-gnu -O2 < %s | FileCheck %s
; RUN: llc -mtriple=aarch64-apple-macosx15.0.0 -O2 < %s | FileCheck %s

; The return-side discriminant must be materialized glued to the RET, AFTER
; the stack-protector cookie check in the epilogue. The old lowering emitted a
; free SUBS ("cmp disc, #1") which the SelectionDAG scheduler could place
; before the epilogue's cookie compare; that compare then became the last
; NZCV def before RET, so the function returned "threw" (C=1 on a matching
; cookie compare) on the success path.
;
; The discriminant now rides on RET_ReallyLR as an operand and the flag is
; materialized inside its expansion, immediately before the ret -- nothing can
; be spliced or scheduled in between.
;
; The `alloca` forces ssp, so the epilogue contains a cookie compare.

declare { { ptr, i64 }, i1 } @callee() #0

define { { ptr, i64 }, i1 } @f() #1 {
; CHECK-LABEL: {{^"?#?_?f"?}}:
; CHECK:      bl {{"?#?_?}}callee{{"?}}
; CHECK:      cset
; CHECK:      cmp x{{[0-9]+}}, x{{[0-9]+}}
; CHECK-NEXT: b.ne
; CHECK:      cmp w{{[0-9]+}}, #1
; CHECK-NEXT: ret
entry:
  %buf = alloca [32 x i8], align 1
  %r = call { { ptr, i64 }, i1 } @callee() #0
  %d = extractvalue { { ptr, i64 }, i1 } %r, 1
  br i1 %d, label %err, label %ok
ok:
  store i8 1, ptr %buf, align 1
  call void asm sideeffect "", "r"(ptr %buf)
  br label %err
err:
  ret { { ptr, i64 }, i1 } %r
}

; Constant success discriminant: materialized as "cmp wzr, #1" (C=0)
; immediately before the ret, after the cookie check.
define { { ptr, i64 }, i1 } @g() #1 {
; CHECK-LABEL: {{^"?#?_?g"?}}:
; CHECK:      cmp x{{[0-9]+}}, x{{[0-9]+}}
; CHECK-NEXT: b.ne
; CHECK:      mov w16, wzr
; CHECK:      cmp w16, #1
; CHECK-NEXT: ret
entry:
  %buf = alloca [48 x i8], align 1
  store i8 1, ptr %buf, align 1
  call void asm sideeffect "", "r"(ptr %buf)
  %v = load i8, ptr %buf, align 1
  %e = sext i8 %v to i64
  %p = insertvalue { ptr, i64 } poison, ptr null, 0
  %q = insertvalue { ptr, i64 } %p, i64 %e, 1
  %r = insertvalue { { ptr, i64 }, i1 } poison, { ptr, i64 } %q, 0
  %r2 = insertvalue { { ptr, i64 }, i1 } %r, i1 false, 1
  ret { { ptr, i64 }, i1 } %r2
}
attributes #0 = { nounwind throws }
attributes #1 = { nounwind ssp throws }
