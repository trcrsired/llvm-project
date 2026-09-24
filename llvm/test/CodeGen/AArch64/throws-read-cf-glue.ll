; RUN: llc -mtriple=aarch64-windows-msvc -O2 < %s | FileCheck %s
; RUN: llc -mtriple=aarch64-unknown-linux-gnu -O2 < %s | FileCheck %s
; RUN: llc -mtriple=arm64ec-pc-windows-msvc -O2 < %s | FileCheck %s

; The caller-side discriminant read (HERB_READ_CF -> HERB_CSET pseudo) must be
; emitted after the throws call whose NZCV.C it reads. The pseudo originally
; had no explicit operands, so the SelectionDAG scheduler saw no dependency
; edge and could place it before the call, where it reads whatever NZCV a
; preceding instruction (here the `icmp`'s `cmp`) left behind.
;
; In this function the flush() call is guarded by a comparison and its
; discriminant merges through a phi into the caller's own return. On the
; buggy codegen the cset landed at the top of the guarded block, before the
; bl, so the call's discriminant was never observed: a successful call looked
; like a failure whenever the leftover carry was set.

declare ptr @iob(i32)
declare { { ptr, i64 }, i1 } @flush() #0

define { { ptr, i64 }, i1 } @g(ptr %p) #0 {
; CHECK-LABEL: {{^"?#?g"?}}:
; CHECK:      b.eq
; CHECK:      bl {{"?#?}}flush{{"?}}
; CHECK-NEXT: cset
entry:
  %s = tail call ptr @iob(i32 noundef 0)
  %eq = icmp eq ptr %p, %s
  br i1 %eq, label %callblk, label %common
callblk:
  %r = tail call { { ptr, i64 }, i1 } @flush() #0
  %d = extractvalue { { ptr, i64 }, i1 } %r, 1
  br i1 %d, label %err, label %common, !prof !1
err:
  %e = extractvalue { { ptr, i64 }, i1 } %r, 0
  %e0 = extractvalue { ptr, i64 } %e, 0
  %e1 = extractvalue { ptr, i64 } %e, 1
  br label %common
common:
  %d.phi = phi i1 [ true, %err ], [ false, %callblk ], [ false, %entry ]
  %p.phi = phi ptr [ %e0, %err ], [ inttoptr (i64 42 to ptr), %callblk ], [ inttoptr (i64 42 to ptr), %entry ]
  %c.phi = phi i64 [ %e1, %err ], [ undef, %callblk ], [ undef, %entry ]
  %ri = insertvalue { ptr, i64 } poison, ptr %p.phi, 0
  %rj = insertvalue { ptr, i64 } %ri, i64 %c.phi, 1
  %r0 = insertvalue { { ptr, i64 }, i1 } poison, { ptr, i64 } %rj, 0
  %r1 = insertvalue { { ptr, i64 }, i1 } %r0, i1 %d.phi, 1
  ret { { ptr, i64 }, i1 } %r1
}
attributes #0 = { nounwind throws }
!1 = !{!"branch_weights", i32 1, i32 1000}
