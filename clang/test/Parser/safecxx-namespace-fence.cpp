// RUN: %clang_cc1 -fsyntax-only -fcxx-exceptions -fsafecxx -verify %s

// A namespace-scope [[unsafe]] { } block is scope-transparent: declarations
// inside stay visible outside, and safe-source restrictions are suppressed
// inside the block only.

[[unsafe]] {
  // Legacy declarations need no mandatory marks inside the fence.
  void legacy_fn() { throw 1; }
  struct Legacy {
    virtual ~Legacy() {}
  };
}

// Names from the fenced block are visible outside (scope transparency).
static void use() noexcept {
  legacy_fn();
  Legacy l;
  (void)l;
}

// Outside the fence, the rules still apply.
void outside() {} // expected-error {{must declare its error channel}} expected-error {{must declare its linkage}}
