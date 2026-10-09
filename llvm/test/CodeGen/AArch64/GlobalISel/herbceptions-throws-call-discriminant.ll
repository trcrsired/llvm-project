; RUN: llc -mtriple=aarch64-unknown-linux-gnu -O0 -global-isel -global-isel-abort=2 -verify-machineinstrs %s -o - | FileCheck %s

; Herbception (throws): the callee returns the {payload, i1} discriminant in
; NZCV.C, not in a return register. A non-throws caller (which does not fall
; back to SelectionDAG like a throws function does) goes through GlobalISel:
; the call site must read the flag (cset .., hs) instead of copying the leaf's
; AAPCS-assigned register (w2), which the callee never writes.

declare { { ptr, i64 }, i1 } @throws_callee(i32) #0

define i32 @caller(i32 %x) {
; CHECK-LABEL: caller:
; CHECK:         bl throws_callee
; CHECK-NOT:     w2
; CHECK:         cset {{w[0-9]+}}, hs
  %r = call { { ptr, i64 }, i1 } @throws_callee(i32 %x) #1
  %d = extractvalue { { ptr, i64 }, i1 } %r, 1
  %s = select i1 %d, i32 -1, i32 0
  ret i32 %s
}

; A discriminant-only return ({i1} flag, void payload) must work too.
declare { i1 } @throws_void(i32) #0

define i32 @caller_flag_only(i32 %x) {
; CHECK-LABEL: caller_flag_only:
; CHECK:         bl throws_void
; CHECK:         cset {{w[0-9]+}}, hs
  %r = call { i1 } @throws_void(i32 %x) #1
  %d = extractvalue { i1 } %r, 0
  %s = select i1 %d, i32 -1, i32 0
  ret i32 %s
}

attributes #0 = { throws }
attributes #1 = { nounwind throws }
