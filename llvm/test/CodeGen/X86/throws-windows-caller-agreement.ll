; RUN: llc -mtriple=x86_64-unknown-windows-msvc -O2 < %s | FileCheck %s

; Herbception (throws): the discriminant is carried in the carry flag and must
; not consume a return-value register. The caller and the callee have to agree
; on that: it is not enough for the callee to return the payload in registers,
; the caller must also expect them there.
;
; This is checked with a caller that is *not* itself a throws function, which is
; where the agreement is easiest to get wrong -- the call side must recognise
; the throws convention from the signature it is calling, not from the function
; it is being compiled into. Getting it wrong makes the caller demand an sret
; pointer for a payload the callee returns in registers: the caller then reads a
; stack slot the callee never wrote.
;
; The payload here is three i64 leaves, which fills RAX/RDX/RCX exactly. If the
; discriminant were also given a slot there would be none left, and the caller
; would silently fall back to sret.

declare { { i64, i64, i64 }, i1 } @callee(i32) #0

; The caller must not pass a hidden pointer: no stack slot is handed to the
; callee as its first argument, and the payload comes back from a register
; rather than being reloaded from memory.
; CHECK-LABEL: caller:
; CHECK-NOT:     leaq {{[0-9]+\(%rsp\)}}, %rcx
; CHECK:         callq callee
; CHECK-NOT:     movq {{[0-9]+\(%rsp\)}}, %rax
; The discriminant is read from CF.
; CHECK:         setb %cl
; CHECK:         retq
define i64 @caller(i32 %n) {
  %c = call { { i64, i64, i64 }, i1 } @callee(i32 %n) #0
  %v = extractvalue { { i64, i64, i64 }, i1 } %c, 0
  %v0 = extractvalue { i64, i64, i64 } %v, 0
  %d = extractvalue { { i64, i64, i64 }, i1 } %c, 1
  %s = select i1 %d, i64 -1, i64 %v0
  ret i64 %s
}

attributes #0 = { throws }
