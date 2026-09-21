; RUN: llc -mtriple=mipsel-unknown-linux-gnu < %s | FileCheck %s --check-prefix=O32
; RUN: llc -mtriple=mips64el-unknown-linux-gnu < %s | FileCheck %s --check-prefix=N64

; A function with the throws (herbception) attribute returns its value with a
; discriminant. On MIPS the discriminant is carried in $a0 (o32) / $a0_64
; (n32/n64), after the $v0:$v1 payload registers, mirroring the RISC-V and
; LoongArch register model.

; Success: discriminant is false (0). Written in the jr delay slot.
define { i64, i1 } @ret_success(i64 %x) #0 {
; O32-LABEL: ret_success:
; O32:            move   $2, $4
; O32-NEXT:       move   $3, $5
; O32-NEXT:       jr     $ra
; O32-NEXT:       addiu  $4, $zero, 0
; N64-LABEL: ret_success:
; N64:            move   $2, $4
; N64-NEXT:       jr     $ra
; N64-NEXT:       daddiu $4, $zero, 0
entry:
  %r.i = insertvalue { i64, i1 } poison, i64 %x, 0
  %r = insertvalue { i64, i1 } %r.i, i1 false, 1
  ret { i64, i1 } %r
}

; Error: discriminant is true (1).
define { i64, i1 } @ret_error(i64 %x) #0 {
; O32-LABEL: ret_error:
; O32:            jr     $ra
; O32-NEXT:       addiu  $4, $zero, 1
; N64-LABEL: ret_error:
; N64:            jr     $ra
; N64-NEXT:       daddiu $4, $zero, 1
entry:
  %r.i = insertvalue { i64, i1 } poison, i64 %x, 0
  %r = insertvalue { i64, i1 } %r.i, i1 true, 1
  ret { i64, i1 } %r
}

; The caller reads the discriminant from $a0 right after the call.
define i64 @call_and_select(i64 %x) #1 {
; O32-LABEL: call_and_select:
; O32:            jal    ret_error
; O32-NEXT:       nop
; O32-NEXT:       andi   $1, $4, 1
; N64-LABEL: call_and_select:
; N64:            jal    ret_error
; N64-NEXT:       nop
; N64-NEXT:       sll    $1, $4, 0
; N64-NEXT:       andi   $1, $1, 1
; N64:            movn
entry:
  %c = call { i64, i1 } @ret_error(i64 %x)
  %val = extractvalue { i64, i1 } %c, 0
  %disc = extractvalue { i64, i1 } %c, 1
  %sel = select i1 %disc, i64 100, i64 %val
  ret i64 %sel
}

; throws_sret: the payload travels through the hidden pointer; the register
; return carries {E, i1} -- the error value plus the discriminant. A real
; error object is pointer-sized pair: {i32,i32} on o32, {i64,i64} on n64.
define { i32, i32, i1 } @ret_sret32(ptr throws_sret({i32,i32,i32,i32,i32}) %out, i32 %x) #0 {
; O32-LABEL: ret_sret32:
; O32:            sw     $5, 0($4)
; O32:            move   $2, $5
; O32:            addiu  $3, $zero, 42
; O32:            jr     $ra
; O32-NEXT:       addiu  $4, $zero, 1
entry:
  store i32 %x, ptr %out, align 4
  %r.i = insertvalue { i32, i32, i1 } poison, i32 %x, 0
  %r.i2 = insertvalue { i32, i32, i1 } %r.i, i32 42, 1
  %r = insertvalue { i32, i32, i1 } %r.i2, i1 true, 2
  ret { i32, i32, i1 } %r
}

define { i64, i64, i1 } @ret_sret64(ptr throws_sret({i64,i64,i64}) %out, i64 %x) #0 {
; N64-LABEL: ret_sret64:
; N64:            sd     $5, 0($4)
; N64:            move   $2, $5
; N64:            daddiu $3, $zero, 42
; N64:            jr     $ra
; N64-NEXT:       daddiu $4, $zero, 1
entry:
  store i64 %x, ptr %out, align 8
  %r.i = insertvalue { i64, i64, i1 } poison, i64 %x, 0
  %r.i2 = insertvalue { i64, i64, i1 } %r.i, i64 42, 1
  %r = insertvalue { i64, i64, i1 } %r.i2, i1 true, 2
  ret { i64, i64, i1 } %r
}

attributes #0 = { throws }
attributes #1 = { nounwind }
