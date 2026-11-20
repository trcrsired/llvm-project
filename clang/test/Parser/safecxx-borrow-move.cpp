// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// A borrow parameter (in/out/inout) is not owned by the callee. Using it as
// the source of a move hides the consumption from the caller: the call site
// shows no consuming parameter, so the caller's CFG cannot diagnose a later
// use. Only move/destructive_move parameters may be consumed.

struct Buf { int x; };
static void consume(destructive_move Buf b) noexcept {}
static void consume_int(destructive_move int b) noexcept {}

// Stand-ins for std::move/std::forward: calls that yield an xvalue.
[[unsafe]] {
namespace testutil {
Buf&& as_rvalue(Buf&) noexcept;
Buf&& as_const_rvalue(const Buf&) noexcept;
int&& as_int_rvalue(int&) noexcept;
}
}

static void in_marker(in Buf b) noexcept {
  consume(destructive_move(b));       // expected-error {{borrow parameter 'b'}}
}
static void in_call(in Buf b) noexcept {
  (void)testutil::as_const_rvalue(b); // expected-error {{borrow parameter 'b'}}
}

static void out_marker(out int b) noexcept {
  b = 1;                               // construct the out parameter
  consume_int(destructive_move(b));   // expected-error {{borrow parameter 'b'}}
}
static void out_call(out int b) noexcept {
  b = 1;
  (void)testutil::as_int_rvalue(b);   // expected-error {{borrow parameter 'b'}}
}

static void inout_marker(inout Buf b) noexcept {
  consume(destructive_move(b));       // expected-error {{borrow parameter 'b'}}
}
static void inout_call(inout Buf b) noexcept {
  (void)testutil::as_rvalue(b);       // expected-error {{borrow parameter 'b'}}
}

// Inside an unsafe fence the same operations are allowed.
static void fenced_marker(inout Buf b) noexcept {
  [[unsafe]] { consume(destructive_move(b)); }
}
static void fenced_call(inout Buf b) noexcept {
  [[unsafe]] { (void)testutil::as_rvalue(b); }
}

// Ordinary locals remain movable in safe source.
static void locals() noexcept {
  Buf a{1};
  consume(destructive_move(a));
}
