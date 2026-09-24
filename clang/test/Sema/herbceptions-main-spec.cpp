// RUN: %clang_cc1 -std=c++26 -fherbceptions -fsyntax-only -verify %s

// 'main' is the outermost frame: there is no caller able to consume a
// {T, i1} error result, so it cannot be declared 'throws' or
// 'return_failure{...}'. An uncaught error in a non-'throws' main()
// traps instead (no destructors guaranteed to run); users who need
// destruction should catch the error with a function-try-block.

struct errc { int v; };
namespace std {
template <typename T> struct error_domain;
template <> struct error_domain<errc> {
  static void *domain() noexcept;
  static int code(errc) noexcept;
};
} // namespace std

int main() throws { // expected-error {{'main' cannot be declared with a herbceptions ('throws'/'return_failure{...}') exception specification; 'main' has no caller able to consume an error result, and uncaught errors in 'main' trap instead}}
  return 0;
}
