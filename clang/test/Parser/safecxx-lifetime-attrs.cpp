// RUN: %clang_cc1 -fsyntax-only -flifetime-protocol -ast-dump %s | FileCheck %s

// P1179 lifetime protocol marks. Both marks are recorded in the AST and,
// under the toggle, translated to Clang's internal lifetime annotations that
// the provenance analysis consumes (see safecxx-provenance.cpp).
// The source label is spelled as a string literal, as is idiomatic for
// Clang C++11 attributes with an argument.

// CHECK: CXXRecordDecl{{.*}} Owner
// CHECK: SafeOwnerAttr
// CHECK: OwnerAttr
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
Owner *use(Owner *p [[lifetime("this")]]) noexcept {
  return p;
}
