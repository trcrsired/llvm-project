// RUN: %clang_cc1 -std=c++20 -fherbceptions -ast-dump -ast-dump-filter=__herb_catch_fails %s | FileCheck %s --check-prefix=AST
// RUN: %clang_cc1 -std=c++20 -fherbceptions -DTEST_INVALID -verify %s

void make_void(bool fail) return_failure{int} {
  if (fail) {
    return_failure 17;
  }
}

using void_result = decltype(catch return_failure(make_void(false)));

// The compiler-owned void specialization has no value alternative. Its
// anonymous union contains only the explicit failure type.
// AST: ClassTemplateSpecializationDecl {{.*}} struct __herb_catch_fails definition
// AST-NOT: FieldDecl {{.*}} value
// AST: FieldDecl {{.*}} error 'int'
// AST: FieldDecl {{.*}} failed 'bool'

#ifdef TEST_INVALID
void reject_void_value_member() {
  auto result = catch return_failure(make_void(false));
  (void)result.value; // expected-error {{no member named 'value'}}
}
#endif
