// RUN: %clang_cc1 -std=c++20 -fherbceptions -ast-dump %s | FileCheck %s

int ownership_source() return_failure{int};
using ownership_result =
    decltype(catch return_failure(ownership_source()));

// An implicit specialization is traversed through its ClassTemplateDecl.
// Registering it independently as a TU lexical child would print it twice and
// give RecursiveASTVisitor clients two apparent parents.
// CHECK-COUNT-1: ClassTemplateSpecializationDecl
// CHECK: TemplateArgument type 'int'
