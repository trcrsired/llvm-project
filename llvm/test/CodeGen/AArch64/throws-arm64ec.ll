; RUN: llc -mtriple=arm64ec-pc-windows-msvc < %s | FileCheck %s

; A function with the throws (herbception) attribute on arm64ec returns its
; payload in x0:x1 -- including 16-byte values such as std::span or
; std::string_view, which the x64-mapped ARM64EC convention would otherwise
; return indirectly -- and the discriminant in NZCV.C, like plain AArch64.
; This applies to EC-internal calls; the x64<->EC thunk boundary does not
; translate NZCV.C to EFLAGS.CF.

; Success: discriminant is false (0) -> C is clear.
define { i64, i1 } @ret_success(i64 %x) #0 {
; CHECK-LABEL: ret_success
; CHECK:            mov w8, wzr
; CHECK-NEXT:       cmp w8, #1
; CHECK-NEXT:       ret
entry:
  %r.i = insertvalue { i64, i1 } poison, i64 %x, 0
  %r = insertvalue { i64, i1 } %r.i, i1 false, 1
  ret { i64, i1 } %r
}

; Error: discriminant is true (1) -> C is set.
define { i64, i1 } @ret_error(i64 %x) #0 {
; CHECK-LABEL: ret_error
; CHECK:            cmp w8, #1
; CHECK:            ret
entry:
  %r.i = insertvalue { i64, i1 } poison, i64 %x, 0
  %r = insertvalue { i64, i1 } %r.i, i1 true, 1
  ret { i64, i1 } %r
}

; A 16-byte payload is returned directly in x0:x1 -- the payload occupies both
; return registers while the discriminant rides in NZCV.C.
define { i64, i64, i1 } @ret_16b(i64 %x, i64 %y) #0 {
; CHECK-LABEL: ret_16b
; CHECK:            mov x8, x0
; CHECK:            mov x0, x1
; CHECK:            cmp w{{[0-9]+}}, #1
; CHECK:            mov x1, x8
; CHECK:            ret
entry:
  %r.i = insertvalue { i64, i64, i1 } poison, i64 %y, 0
  %r.i2 = insertvalue { i64, i64, i1 } %r.i, i64 %x, 1
  %r = insertvalue { i64, i64, i1 } %r.i2, i1 false, 2
  ret { i64, i64, i1 } %r
}

; The caller selects on the discriminant straight off NZCV.C.
define i64 @call_and_select(i64 %x) #1 {
; CHECK-LABEL: call_and_select
; CHECK:            bl "#ret_error"
; CHECK:            csel x0, {{x[0-9]+}}, x0, hs
entry:
  %c = call { i64, i1 } @ret_error(i64 %x)
  %val = extractvalue { i64, i1 } %c, 0
  %disc = extractvalue { i64, i1 } %c, 1
  %sel = select i1 %disc, i64 100, i64 %val
  ret i64 %sel
}

; throws_sret: the payload travels through the hidden pointer; the register
; return carries {E, i1} -- a 16-byte error value in x0:x1 plus the
; discriminant in NZCV.C.
define { i64, i64, i1 } @ret_sret64(ptr throws_sret({i64,i64,i64}) %out, i64 %x) #0 {
; CHECK-LABEL: ret_sret64
; CHECK:            cmp w{{[0-9]+}}, #1
; CHECK:            mov x0, x1
; CHECK:            mov {{w|x}}1, #42
; CHECK:            str {{x[0-9]+}}, [{{x[0-9]+}}]
; CHECK:            ret
entry:
  store i64 %x, ptr %out, align 8
  %r.i = insertvalue { i64, i64, i1 } poison, i64 %x, 0
  %r.i2 = insertvalue { i64, i64, i1 } %r.i, i64 42, 1
  %r = insertvalue { i64, i64, i1 } %r.i2, i1 true, 2
  ret { i64, i64, i1 } %r
}

attributes #0 = { throws }
attributes #1 = { nounwind }
