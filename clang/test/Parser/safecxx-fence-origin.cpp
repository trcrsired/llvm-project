// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify -I %S/Inputs %s
// expected-no-diagnostics

// A fence that a library opens inside its own header is a syntax relaxation
// for its kernel, not a legacy import by the user. Its declarations stay
// callable; dangerous APIs are classified explicitly with [[unsafe]].
#include "safecxx-self-fenced.h"

static void use_widget() noexcept {
  Widget w;
  (void)w.size();
}

// A fence is a source *range*, not a whole file: code after the closing brace
// is checked again.
[[unsafe]] {
static int *kernel_storage;
}

static void after_fence() noexcept {
  int *p = nullptr; // OK: pointers are opaque tokens in safe source
  (void)p;
}
