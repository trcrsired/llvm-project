; RUN: llc -mtriple=powerpc-unknown-linux-gnu < %s | FileCheck %s --check-prefix=PPC32
; RUN: llc -mtriple=powerpc64le-unknown-linux-gnu < %s | FileCheck %s --check-prefix=PPC64

; A function with the throws (herbception) attribute returns its value with a
; discriminant. On PowerPC the payload is returned in r3:r4 and the
; discriminant in cr6: the callee writes the field with cmpwi glued to blr,
; cr6.GT (bit 25) = error, cr6.EQ = success. The caller tests the CR bit
; directly with bc/isel -- no mfocrf materialization is needed.

; Success: discriminant is false (0) -> cr6.EQ.
define { i64, i1 } @ret_success(i64 %x) #0 {
; PPC32-LABEL: ret_success:
; PPC32:            cmpwi 6, {{[0-9]+}}, 0
; PPC32:            blr
; PPC64-LABEL: ret_success:
; PPC64:            li 4, 0
; PPC64-NEXT:       cmpwi 6, 4, 0
; PPC64:            blr
entry:
  %r.i = insertvalue { i64, i1 } poison, i64 %x, 0
  %r = insertvalue { i64, i1 } %r.i, i1 false, 1
  ret { i64, i1 } %r
}

; Error: discriminant is true (1) -> cr6.GT.
define { i64, i1 } @ret_error(i64 %x) #0 {
; PPC32-LABEL: ret_error:
; PPC32:            li {{[0-9]+}}, 1
; PPC32:            cmpwi 6, {{[0-9]+}}, 0
; PPC32:            blr
; PPC64-LABEL: ret_error:
; PPC64:            cmpwi 6, {{[0-9]+}}, 0
; PPC64:            blr
entry:
  %r.i = insertvalue { i64, i1 } poison, i64 %x, 0
  %r = insertvalue { i64, i1 } %r.i, i1 true, 1
  ret { i64, i1 } %r
}

; The caller branches on the cr6 bit (bit 25 = cr6.GT) immediately after the
; call, with no register round-trip.
define i64 @call_and_branch(i64 %x) #1 {
; PPC32-LABEL: call_and_branch:
; PPC32:            bl ret_error
; PPC32-NEXT:       bc {{[0-9]+}}, 25,
; PPC64-LABEL: call_and_branch:
; PPC64:            bl ret_error
; PPC64:            bc {{[0-9]+}}, 25,
entry:
  %c = call { i64, i1 } @ret_error(i64 %x)
  %val = extractvalue { i64, i1 } %c, 0
  %disc = extractvalue { i64, i1 } %c, 1
  br i1 %disc, label %err, label %ok
err:
  ret i64 -1
ok:
  ret i64 %val
}

; A select on the discriminant becomes an isel on the cr6.GT bit.
define i64 @call_and_select(i64 %x) #1 {
; PPC64-LABEL: call_and_select:
; PPC64:            bl ret_error
; PPC64:            isel 3, {{[0-9]+}}, 3, 25
entry:
  %c = call { i64, i1 } @ret_error(i64 %x)
  %val = extractvalue { i64, i1 } %c, 0
  %disc = extractvalue { i64, i1 } %c, 1
  %sel = select i1 %disc, i64 100, i64 %val
  ret i64 %sel
}

; throws_sret: the payload travels through the hidden pointer; the register
; return carries {E, i1} -- the error value in r3:r4 plus the discriminant in
; cr6.GT.
define { i64, i64, i1 } @ret_sret64(ptr throws_sret({i64,i64,i64}) %out, i64 %x) #0 {
; PPC64-LABEL: ret_sret64:
; PPC64:            std 4, 0(3)
; PPC64:            cmpwi 6, {{[0-9]+}}, 0
; PPC64:            mr 3, 4
; PPC64:            li 4, 42
; PPC64:            blr
entry:
  store i64 %x, ptr %out, align 8
  %r.i = insertvalue { i64, i64, i1 } poison, i64 %x, 0
  %r.i2 = insertvalue { i64, i64, i1 } %r.i, i64 42, 1
  %r = insertvalue { i64, i64, i1 } %r.i2, i1 true, 2
  ret { i64, i64, i1 } %r
}

attributes #0 = { throws }
attributes #1 = { nounwind }
