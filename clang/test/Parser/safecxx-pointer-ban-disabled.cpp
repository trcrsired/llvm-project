// RUN: %clang_cc1 -fsyntax-only %s
//
// Without -fsafecxx the pointer/reference ban is not active, and C++ source
// is unchanged. This mirrors safecxx-parameter-passing-disabled.cpp.

static int *raw_return(int *p) { return p; }
static void raw_ref(int &r) { r = 1; }
struct Raw { int *ptr; };
