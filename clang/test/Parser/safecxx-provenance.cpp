// RUN: %clang_cc1 -fsyntax-only -Wno-return-stack-address -fsafecxx -verify %s
// RUN: %clang_cc1 -fsyntax-only -Wno-return-stack-address -flifetime-protocol -verify %s
// RUN: %clang_cc1 -fsyntax-only -Wno-return-stack-address %s

// P1179 provenance: our two public marks are translated to Clang's internal
// lifetime annotations and the lifetime-safety engine enforces the rule that a
// borrow is valid only while every owner in its provenance is alive.
//
// [[owner]] says a class's pointer fields are owned by the object itself.
// [[lifetime("this")]] on a constructor parameter says the constructed view's
// pointer fields borrow from that parameter.
//
// The two library types below are implementation internals, so their raw
// pointer fields and reference parameters are written inside a trusted-unsafe
// fence. The provenance engine still runs inside the fence; only the
// source-level pointer/reference ban is suppressed.

[[unsafe]] {
struct [[owner]] my_string {
  const char* ptr_;
  my_string() noexcept : ptr_(nullptr) {}
  ~my_string() noexcept {}
  my_string(const my_string&) = delete;
  my_string& operator=(const my_string&) = delete;
};

struct my_view {
  const char* ptr_;
  my_view(const my_string& s [[lifetime("this")]]) noexcept : ptr_(s.ptr_) {}
};

// A view escaping a local owner is diagnosed: the local dies first.
static my_view bad_local() noexcept {
  my_string local;
  // expected-error@+2 {{stack memory associated with local variable 'local' is returned}}
  // expected-note@+1 {{returned here}}
  return my_view(local);
}

// A view of a temporary owner is diagnosed: the temporary dies at the end of
// the full-expression.
static my_view bad_temp() noexcept {
  // expected-error@+2 {{stack memory associated with temporary object is returned}}
  // expected-note@+1 {{returned here}}
  return my_view(my_string{});
}

// A view of an owner that outlives the call is fine.
static void good(const my_string& s) noexcept {
  my_view v(s);
  (void)v;
}
} // end trusted-unsafe library implementation

// Without -fsafecxx / -flifetime-protocol the marks are inert and the engine
// stays silent (see the third RUN line, which has no expected diagnostics).
