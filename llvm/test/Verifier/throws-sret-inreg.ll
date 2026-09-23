; RUN: llvm-as %s -o - | llvm-dis | FileCheck %s

; The MSVC AArch64/arm64ec ABI classifier marks indirect non-trivial record
; returns 'inreg' so the hidden return pointer uses the Windows alternate
; placement (x0 for free functions, x1 for instance methods) instead of the
; AAPCS x8 slot. The frontend propagates that onto the throws_sret parameter,
; so 'inreg throws_sret' must be accepted like 'inreg sret' is — otherwise the
; combination cannot survive bitcode round-tripping or ThinLTO import.

%struct.NT = type { ptr, ptr, ptr }

; CHECK: define {{.*}}@callee(ptr inreg noalias writable throws_sret(%struct.NT)
define { { ptr, i64 }, i1 } @callee(ptr inreg noalias writable throws_sret(%struct.NT) %out, i32 %n) {
  ret { { ptr, i64 }, i1 } zeroinitializer
}

; CHECK: call {{.*}}@callee(ptr inreg {{.*}}throws_sret(%struct.NT)
define { { ptr, i64 }, i1 } @caller(ptr %slot, i32 %n) {
  %r = call { { ptr, i64 }, i1 } @callee(ptr inreg noalias writable throws_sret(%struct.NT) %slot, i32 %n)
  ret { { ptr, i64 }, i1 } %r
}
