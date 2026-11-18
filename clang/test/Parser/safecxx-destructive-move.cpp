// RUN: %clang_cc1 -fsyntax-only -fparameter-passing -verify %s

// destructive_move is a bitwise relocation (copy object representation, skip
// the source destructor), so it is only valid for trivially relocatable types.

struct Trivial { int a, b; };
struct NonTrivial {
  NonTrivial(const NonTrivial &) {}
  ~NonTrivial() {}
};

void ok_scalar(destructive_move int x) noexcept {}
void ok_record(destructive_move Trivial x) noexcept {}

// expected-error@+1 {{destructive_move parameter of type 'NonTrivial' requires a trivially relocatable type}}
void bad(destructive_move NonTrivial x) noexcept {}

// Dependent types are not checked until instantiation.
template <class T> void tmpl(destructive_move T x) noexcept {}
void use_tmpl() noexcept { tmpl(1); }
