// RUN: %clang_cc1 -std=c++26 -fherbceptions -fsyntax-only -verify %s

// A placement (replacement) new-expression whose constructor is declared
// 'throws' returns the error through the herbception channel. Constructor
// calls are never wrapped in try(expr); CodeGen auto-propagates the error in
// a throws function, traps it in main(), and routes it to an enclosing
// `catch throws` handler. Other non-throws contexts are diagnosed in Sema so
// that -fsyntax-only catches them.

namespace std {
struct error {
  void const *domain;
  __SIZE_TYPE__ code;
};
enum class byte : unsigned char {};
} // namespace std

// The non-allocating placement form, normally declared by <new>.
inline void *operator new(__SIZE_TYPE__, void *p) noexcept { return p; }

struct throwing_ctor {
  throwing_ctor() throws;
  throwing_ctor(int) throws;
  ~throwing_ctor() = default;
  int v;
};

struct non_throwing {
  non_throwing() = default;
  ~non_throwing() = default;
  int v = 0;
};

// throws function: the throwing constructor auto-propagates.
void ok_throws() throws {
  alignas(throwing_ctor) char buf[sizeof(throwing_ctor)];
  ::new (buf) throwing_ctor();
  ::new (buf) throwing_ctor(1);
}

// main(): an escaped error traps at runtime.
int main() {
  alignas(throwing_ctor) char buf[sizeof(throwing_ctor)];
  ::new (buf) throwing_ctor();
  return 0;
}

void bad_noexcept() noexcept {
  alignas(throwing_ctor) char buf[sizeof(throwing_ctor)];
  ::new (buf) throwing_ctor(); // expected-error {{call to 'throws' function in a non-'throws' function must be handled by an enclosing 'try { } catch throws' block, or mark the calling function as 'throws' so herbceptions can propagate}}
}

void bad_plain() {
  alignas(throwing_ctor) char buf[sizeof(throwing_ctor)];
  ::new (buf) throwing_ctor(1); // expected-error {{call to 'throws' function in a non-'throws' function must be handled by an enclosing 'try { } catch throws' block, or mark the calling function as 'throws' so herbceptions can propagate}}
}

// Other constructor forms are checked the same way.
void bad_decl() noexcept {
  throwing_ctor x; // expected-error {{call to 'throws' function in a non-'throws' function must be handled by an enclosing 'try { } catch throws' block, or mark the calling function as 'throws' so herbceptions can propagate}}
  (void)x;
}

void bad_temporary() noexcept {
  (void)throwing_ctor(1); // expected-error {{call to 'throws' function in a non-'throws' function must be handled by an enclosing 'try { } catch throws' block, or mark the calling function as 'throws' so herbceptions can propagate}}
}

struct bad_member {
  throwing_ctor m;
  bad_member() noexcept : m() {} // expected-error {{call to 'throws' function in a non-'throws' function must be handled by an enclosing 'try { } catch throws' block, or mark the calling function as 'throws' so herbceptions can propagate}}
};

// A `catch throws` handler routes a throwing constructor in the try body.
void ok_try_block() noexcept {
  try {
    throwing_ctor x(1);
    (void)x;
  } catch throws(::std::error e) {
    (void)e;
  }
}

// Replacement new is constexpr (P2747): a non-throwing constructor can be
// evaluated at compile time through placement new over an existing object.
constexpr int const_new() {
  non_throwing a;
  a.v = 3;
  non_throwing *p = ::new (&a) non_throwing();
  p->v = 7;
  return p->v;
}
static_assert(const_new() == 7);
