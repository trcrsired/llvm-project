; RUN: llvm-as < %s | llvm-dis | FileCheck %s
; RUN: verify-uselistorder < %s

; throws_sret must survive an assembly -> bitcode -> assembly round trip,
; including its carried pointee type, on declarations, definitions and calls.

; CHECK: define { ptr, i64 } @ret_payload(ptr throws_sret({ i64, i64, i64, i64 }) %out, i32 %n)
define { ptr, i64 } @ret_payload(ptr throws_sret({ i64, i64, i64, i64 }) %out, i32 %n) {
entry:
  store i64 1, ptr %out, align 8
  ret { ptr, i64 } zeroinitializer
}

; CHECK: declare { ptr, i64 } @decl(ptr throws_sret({ i64, i64, i64, i64 }))
declare { ptr, i64 } @decl(ptr throws_sret({ i64, i64, i64, i64 }))

; CHECK: call { ptr, i64 } @decl(ptr throws_sret({ i64, i64, i64, i64 }) %
define { ptr, i64 } @caller(ptr throws_sret({ i64, i64, i64, i64 }) %out) {
  %r = call { ptr, i64 } @decl(ptr throws_sret({ i64, i64, i64, i64 }) %out)
  ret { ptr, i64 } %r
}
