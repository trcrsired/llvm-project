; RUN: llc -mtriple=xtensa < %s | FileCheck %s --check-prefix=CALL0
; RUN: llc -mtriple=xtensa -mattr=+windowed < %s | FileCheck %s --check-prefix=WIN

; A function with the throws (herbception) attribute returns its value with a
; discriminant. Xtensa has no practical integer-to-Boolean-register move for
; b0, so the discriminant is carried in an integer register: a4 in the call0
; ABI. Under the windowed ABI the callee still writes its a4, which the caller
; observes as a12 after the register-window rotation.

; Success: discriminant is false (0).
define { i32, i1 } @ret_success(i32 %x) #0 {
; CALL0-LABEL: ret_success:
; CALL0:            movi a4, 0
; CALL0-NEXT:       ret
; WIN-LABEL: ret_success:
; WIN:            movi a4, 0
; WIN:            retw
entry:
  %r.i = insertvalue { i32, i1 } poison, i32 %x, 0
  %r = insertvalue { i32, i1 } %r.i, i1 false, 1
  ret { i32, i1 } %r
}

; Error: discriminant is true (1).
define { i32, i1 } @ret_error(i32 %x) #0 {
; CALL0-LABEL: ret_error:
; CALL0:            movi a4, 1
; CALL0-NEXT:       ret
; WIN-LABEL: ret_error:
; WIN:            movi a4, 1
; WIN:            retw
entry:
  %r.i = insertvalue { i32, i1 } poison, i32 %x, 0
  %r = insertvalue { i32, i1 } %r.i, i1 true, 1
  ret { i32, i1 } %r
}

; The caller reads the discriminant: a4 under call0, a12 under the windowed
; ABI (the callee's a4 rotated into the caller's frame).
define i32 @call_and_branch(i32 %x) #1 {
; CALL0-LABEL: call_and_branch:
; CALL0:            callx0
; CALL0:            and  a{{[0-9]+}}, a4, a{{[0-9]+}}
; CALL0:            b{{eqz|nez}} a{{[0-9]+}},
; WIN-LABEL: call_and_branch:
; WIN:            callx8
; WIN:            and  a{{[0-9]+}}, a12, a{{[0-9]+}}
entry:
  %c = call { i32, i1 } @ret_error(i32 %x)
  %val = extractvalue { i32, i1 } %c, 0
  %disc = extractvalue { i32, i1 } %c, 1
  br i1 %disc, label %err, label %ok
err:
  ret i32 -1
ok:
  ret i32 %val
}

; throws_sret: the payload travels through the hidden pointer; the register
; return carries {E, i1} -- the error value in a2:a3 plus the discriminant in
; a4 (call0).
define { i32, i32, i1 } @ret_sret32(ptr throws_sret({i32,i32,i32,i32,i32}) %out, i32 %x) #0 {
; CALL0-LABEL: ret_sret32:
; CALL0:            s32i {{.*}}, a2, 0
; CALL0:            movi a3, 42
; CALL0:            movi a4, 1
; CALL0:            ret
entry:
  store i32 %x, ptr %out, align 4
  %r.i = insertvalue { i32, i32, i1 } poison, i32 %x, 0
  %r.i2 = insertvalue { i32, i32, i1 } %r.i, i32 42, 1
  %r = insertvalue { i32, i32, i1 } %r.i2, i1 true, 2
  ret { i32, i32, i1 } %r
}

attributes #0 = { throws }
attributes #1 = { nounwind }
