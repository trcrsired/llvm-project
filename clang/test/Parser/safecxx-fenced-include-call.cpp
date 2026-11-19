// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// A namespace-scope [[unsafe]] fence may wrap an #include. The imported
// declarations are legacy code: they are exempt from the safe-source-only
// rules, and they are unsafe to call from safe source. A safe caller must
// re-enter an [[unsafe]] { } block.

[[unsafe]] {
#include "Inputs/safecxx-fenced-legacy.h"
}

static void safe_caller() noexcept {
  legacy_entry(); // expected-error {{call to 'legacy_entry' is unsafe}}
}

static void fenced_caller() noexcept {
  [[unsafe]] {
    legacy_entry();
  }
}

// A call inside the fenced header itself (legacy_helper() in legacy_entry())
// is not diagnosed.
