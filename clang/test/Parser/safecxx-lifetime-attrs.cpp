// RUN: %clang_cc1 -fsyntax-only -flifetime-protocol -ast-dump %s | FileCheck %s

// P1179 lifetime protocol marks. The attributes are recorded in the AST;
// the provenance analysis is a later step.
// The source label is spelled as a string literal, as is idiomatic for
// Clang C++11 attributes with an argument.

// CHECK: CXXRecordDecl{{.*}} Owner
// CHECK: SafeOwnerAttr
struct [[owner]] Owner {
  int *ptr;
  Owner(Owner *src [[lifetime("this")]]) noexcept : ptr(src->ptr) {}
  Owner(const Owner&) = delete;
  Owner& operator=(const Owner&) = delete;
  ~Owner() noexcept {}
};

// CHECK: ParmVarDecl{{.*}} src 'Owner *'
// CHECK-NEXT: LifetimeAttr{{.*}} "this"

// CHECK: ParmVarDecl{{.*}} p 'Owner *'
// CHECK-NEXT: LifetimeAttr{{.*}} "this"
void use(Owner *p [[lifetime("this")]]) noexcept {
  (void)p;
}
