// RUN: %clang_cc1 -fsyntax-only -fparameter-passing -ast-dump %s | FileCheck %s --check-prefix=PP

// The d0708 parameter-passing keywords are contextual: with the flag they are
// recognized only in parameter decl-specifier position. The kind is preserved
// on the ParmVarDecl. `in T` lowers to a by-value `const T` when T is cheap to
// copy (scalars and small trivially-copyable records), else to `const T&`.

// PP: FunctionDecl{{.*}} f_in
// PP-NEXT: ParmVarDecl{{.*}} x 'const int' in
void f_in(in int x) noexcept {}

struct Small { int a, b; };       // 8 bytes: cheap
struct Big { char buf[64]; };     // 64 bytes: const ref

// PP: FunctionDecl{{.*}} f_in_small
// PP-NEXT: ParmVarDecl{{.*}} s 'const Small' in
void f_in_small(in Small s) noexcept {}

// PP: FunctionDecl{{.*}} f_in_big
// PP-NEXT: ParmVarDecl{{.*}} b 'const Big &' in
void f_in_big(in Big b) noexcept {}

// PP: FunctionDecl{{.*}} f_out
// PP-NEXT: ParmVarDecl{{.*}} y 'int &' out
void f_out(out int y) noexcept { y = 1; }

// PP: FunctionDecl{{.*}} f_inout
// PP-NEXT: ParmVarDecl{{.*}} z 'int &' inout
void f_inout(inout int z) noexcept {}

// PP: FunctionDecl{{.*}} f_move
// PP-NEXT: ParmVarDecl{{.*}} w 'int &&' move
void f_move(move int w) noexcept {}

// PP: FunctionDecl{{.*}} f_dmove
// PP-NEXT: ParmVarDecl{{.*}} v 'int' destructive_move
void f_dmove(destructive_move int v) noexcept {}

// Ordinary parameters are unaffected.
// PP: FunctionDecl{{.*}} f_plain
// PP-NEXT: ParmVarDecl{{.*}} p 'int'
void f_plain(int p) noexcept {}

// Outside parameter position the words are still ordinary identifiers.
// PP: VarDecl{{.*}} in 'int'
int in = 0;

void use_names() noexcept {
  int in = 5;
  in = in + 1;
  (void)in;
}
