// RUN: %clang_cc1 -std=c++26 -fherbceptions -fcxx-exceptions -fexceptions -fsyntax-only -verify %s
// expected-no-diagnostics

// A bare `throws` function and a `try { } catch throws(std::error e)` block
// implicitly convert any escaping legacy C++ exception (thrown by a
// noexcept(false) callee) into a fabricated std::error through the user's
// error_domain<std::exception_ptr> specialization. When the user has not
// provided that specialization (e.g. the exception_ptr header is not
// included), the conversion must degrade silently: the throws function simply
// cannot capture legacy exceptions. Referencing error_domain<exception_ptr>
// must NOT trigger an "implicit instantiation of undefined template" error
// against the (undefined) primary template.

namespace std {
struct exception_ptr {};                 // stub; no error_domain<exception_ptr> below
class error { public: ~error() noexcept; };
template <typename T> class error_domain; // primary template, no definition
} // namespace std

extern void legacy_callee() noexcept(false);

// Bare `throws`: ActOnFinishFunctionBody fabricates the legacy-conversion
// expression. Without error_domain<std::exception_ptr> it must return
// ExprError silently (no diagnostic).
void throws_no_specialization() throws {
  legacy_callee();
}

// `catch throws(std::error)`: ActOnCXXCatchThrowsHandler fabricates the same
// conversion expression. Same silent degradation.
void catch_no_specialization() {
  try {
    legacy_callee();
  } catch throws(::std::error e) {
    (void)e;
  }
}

// A declaration (no body) is likewise fine.
void decl_only() throws;
