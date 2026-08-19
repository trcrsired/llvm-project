// RUN: %clang_cc1 -std=c++26 -fherbceptions -fcxx-exceptions -fexceptions -emit-llvm -o /dev/null -verify %s

// A bare `throws` function implicitly converts any legacy C++ exception that
// escapes it (thrown by a `noexcept(false)` callee) into a fabricated
// std::error on the herbception channel, via the user's
// error_domain<std::exception_ptr> specialization. Two cases:
//
//  1. The `throws` function only calls `throws`/`noexcept` callees: no legacy
//     exception can escape, so the conversion is not needed. When the
//     error_domain<std::exception_ptr> specialization is missing (e.g. the
//     exception_ptr header is not included) this must compile silently —
//     referencing error_domain<exception_ptr> must NOT trigger an "implicit
//     instantiation of undefined template" error against the primary template.
//
//  2. The `throws` function calls a `noexcept(false)` callee: a legacy C++
//     exception can escape, so the conversion IS needed. When the
//     error_domain<std::exception_ptr> specialization is missing this is a
//     hard error: the user must provide the domain (include the exception_ptr
//     error_domain header) or disable C++ exceptions with -fno-exceptions.

namespace std {
struct exception_ptr {};                  // stub; no error_domain<exception_ptr> below
class error { public: ~error() noexcept; };
template <typename T> class error_domain; // primary template, no definition
} // namespace std

extern void throws_callee() throws;       // noexcept (throws implies noexcept)
extern void legacy_callee() noexcept(false);

// Case 1: only noexcept/throws callees — no legacy escape possible. With no
// error_domain<std::exception_ptr> specialization, this compiles silently.
void only_throws_calls() throws {
  throws_callee();
}

// A declaration (no body) is likewise fine.
void decl_only() throws;

// Case 2: a `noexcept(false)` callee can throw a legacy C++ exception that
// would escape the `throws` function. Without error_domain<std::exception_ptr>
// the conversion is unavailable, so this must fail to compile.
void calls_legacy() throws { // expected-error {{a 'throws' function can convert an escaping legacy C++ exception into a herbception only through std::error_domain<std::exception_ptr>, which is not defined; include the exception_ptr error_domain header or compile with -fno-exceptions to disable C++ exceptions}}
  legacy_callee();
}
