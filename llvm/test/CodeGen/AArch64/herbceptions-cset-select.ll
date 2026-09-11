; RUN: llc -mtriple=aarch64-unknown-linux-gnu -O2 < %s | FileCheck %s

; Herbceptions (throws): a callee returns its failure discriminant in NZCV.C, and
; clang materialises it right after the call with a HERB_CSET and tests bit 0 of
; it with an ands. When that test feeds a select there is nothing that has to
; keep the intermediate value, so the cset and the ands fold away and the csel
; tests the carry the call left behind itself. This mirrors
; herbceptions-cset-flags-clobber.ll, which covers the same discriminant feeding
; a branch.

declare { i32, i1 } @callee(i32) #0
declare { { ptr, i64 }, i1 } @callee_pair(i32 noundef) #0
declare void @capture(i32)

; The discriminant selected on directly: the cset/tst pair collapses into the
; csel. C set means the callee reported failure, so the true arm (100) keeps the
; hs condition.
define i32 @select_on_disc(i32 %x) #1 {
; CHECK-LABEL: select_on_disc:
; CHECK:         bl callee
; CHECK-NEXT:    mov w8, #100
; CHECK-NEXT:    csel w0, w8, w0, hs
; CHECK-NOT:     cset
; CHECK-NOT:     tst
  %c = call { i32, i1 } @callee(i32 %x) #0
  %v = extractvalue { i32, i1 } %c, 0
  %d = extractvalue { i32, i1 } %c, 1
  %s = select i1 %d, i32 100, i32 %v
  ret i32 %s
}

; The same fold with the select arms swapped, which has to flip the condition:
; the true arm is now the callee's value, so 100 is selected when C is clear.
define i32 @select_inverted(i32 %x) #1 {
; CHECK-LABEL: select_inverted:
; CHECK:         bl callee
; CHECK-NEXT:    mov w8, #100
; CHECK-NEXT:    csel w0, w0, w8, hs
; CHECK-NOT:     cset
; CHECK-NOT:     tst
  %c = call { i32, i1 } @callee(i32 %x) #0
  %v = extractvalue { i32, i1 } %c, 0
  %d = extractvalue { i32, i1 } %c, 1
  %s = select i1 %d, i32 %v, i32 100
  ret i32 %s
}

; A flag-setting instruction between the cset and the test redefines NZCV, so
; the cset is no longer the live definition of the carry at the select and the
; fold must not fire: erasing the pair would leave the csel reading this cmp.
; The cset/ands/csel sequence has to survive intact.
define i32 @select_clobber(i32 %x, i32 %y) #1 {
; CHECK-LABEL: select_clobber:
; CHECK:         bl callee
; CHECK-NEXT:    cset w8, hs
; CHECK-NEXT:    cmp w0, w19
; CHECK-NEXT:    csel w9, w0, w19, gt
; CHECK-NEXT:    tst w8, #0x1
; CHECK-NEXT:    mov w8, #100
; CHECK-NEXT:    csel w0, w8, w9, ne
  %c = call { i32, i1 } @callee(i32 %x) #0
  %v = extractvalue { i32, i1 } %c, 0
  %d = extractvalue { i32, i1 } %c, 1
  %cmp = icmp sgt i32 %v, %y
  %sv = select i1 %cmp, i32 %v, i32 %y
  %s = select i1 %d, i32 100, i32 %sv
  ret i32 %s
}

; Two selects reading the same discriminant share the flags the test sets, so
; the fold must not fire: rewriting only one of them would leave the other
; testing whatever NZCV the call left behind. The cset/tst pair stays and both
; csels keep reading it.
define i32 @two_readers(i32 %x) #1 {
; CHECK-LABEL: two_readers:
; CHECK:         bl callee
; CHECK-NEXT:    cset w8, hs
; CHECK-NEXT:    mov w9, #200
; CHECK-NEXT:    tst w8, #0x1
; CHECK-NEXT:    mov w8, #100
; CHECK-NEXT:    csel w8, w8, w0, ne
; CHECK-NEXT:    csel w0, w9, w8, ne
  %c = call { i32, i1 } @callee(i32 %x) #0
  %v = extractvalue { i32, i1 } %c, 0
  %d = extractvalue { i32, i1 } %c, 1
  %a = select i1 %d, i32 100, i32 %v
  %b = select i1 %d, i32 200, i32 %a
  ret i32 %b
}

; The branch form of the same discriminant still folds (visitHERB_CSET), in both
; polarities: the error path is taken on C set, and branching the other way is
; the same test with the condition reversed.
define void @branch_on_disc(i32 %x) #1 {
; CHECK-LABEL: branch_on_disc:
; CHECK:         bl callee
; CHECK-NEXT:    b.lo
  %c = call { i32, i1 } @callee(i32 %x) #0
  %d = extractvalue { i32, i1 } %c, 1
  br i1 %d, label %err, label %ok
err:
  call void @capture(i32 1)
  ret void
ok:
  ret void
}

define void @branch_inverted(i32 %x) #1 {
; CHECK-LABEL: branch_inverted:
; CHECK:         bl callee
; CHECK-NEXT:    b.hs
  %c = call { i32, i1 } @callee(i32 %x) #0
  %d = extractvalue { i32, i1 } %c, 1
  br i1 %d, label %ok, label %err
err:
  call void @capture(i32 1)
  ret void
ok:
  ret void
}

; The shape try(expr) propagation actually produces: the discriminant is
; selected on *and* returned as the caller's own discriminant, so it has a use
; besides the test. Folding must still drop the test - the cset is kept because
; the returned discriminant is derived from it by the cmp.
define { { ptr, i64 }, i1 } @propagate_reuses_discriminant(i32 noundef %0) #0 {
; CHECK-LABEL: propagate_reuses_discriminant:
; CHECK:         bl callee_pair
; CHECK-NEXT:    cset w8, hs
; CHECK-NEXT:    mov w9, w0
; CHECK-NEXT:    csel x0, x0, x9, hs
; CHECK-NEXT:    cmp w8, #1
; CHECK-NOT:     tst
  %c = tail call { { ptr, i64 }, i1 } @callee_pair(i32 noundef %0) #0
  %v = extractvalue { { ptr, i64 }, i1 } %c, 0
  %d = extractvalue { { ptr, i64 }, i1 } %c, 1
  %p = extractvalue { ptr, i64 } %v, 0
  %n = extractvalue { ptr, i64 } %v, 1
  %pi = ptrtoint ptr %p to i64
  %m = and i64 %pi, 4294967295
  %z = inttoptr i64 %m to ptr
  %s = select i1 %d, ptr %p, ptr %z
  %r0 = insertvalue { ptr, i64 } poison, ptr %s, 0
  %r1 = insertvalue { ptr, i64 } %r0, i64 %n, 1
  %o0 = insertvalue { { ptr, i64 }, i1 } poison, { ptr, i64 } %r1, 0
  %o1 = insertvalue { { ptr, i64 }, i1 } %o0, i1 %d, 1
  ret { { ptr, i64 }, i1 } %o1
}

attributes #0 = { throws }
attributes #1 = { nounwind }
