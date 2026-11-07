; RUN: not llvm-as %s -o /dev/null 2>&1 | FileCheck %s

; herbthrows_sret is the herbception counterpart of sret: it names a hidden
; pointer through which the payload is returned, but unlike sret it does not
; force the function's return type to void.

; Unlike 'sret', a non-void return is legal.
define i64 @ok_non_void(ptr herbthrows_sret(i64) %out) {
  ret i64 0
}

declare i64 @decl_non_void(ptr herbthrows_sret(i64))

define i64 @call_non_void(ptr herbthrows_sret(i64) %out) {
  %r = call i64 @decl_non_void(ptr herbthrows_sret(i64) %out)
  ret i64 %r
}

; CHECK: Cannot have multiple 'herbthrows_sret' parameters!
declare i64 @two(ptr herbthrows_sret(i64) %a, ptr herbthrows_sret(i64) %b)

; CHECK: Attribute 'herbthrows_sret' is not on first or second parameter!
declare i64 @late(ptr %a, ptr %b, ptr herbthrows_sret(i64) %c)

; CHECK: Attributes 'byval', 'inalloca', 'preallocated', 'inreg', 'nest', 'byref', 'sret', and 'herbthrows_sret' are incompatible!
declare i64 @byval(ptr herbthrows_sret(i64) byval(i64) %p)

; CHECK: Attributes 'herbthrows_sret and returned' are incompatible!
declare i64 @returned(ptr herbthrows_sret(i64) returned %p)

; CHECK: herbthrows_sret(i64)' applied to incompatible type!
; CHECK-NEXT: ptr @not_ptr
declare i64 @not_ptr(i64 herbthrows_sret(i64) %x)
