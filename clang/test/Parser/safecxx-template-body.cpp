// RUN: %clang_cc1 -fsyntax-only -fsafecxx -verify %s

// The definite-first-use and definite-last-use checks run on compiled bodies:
// on an instantiation of a function template, not on its pattern. Skipping
// both (the pattern because it is dependent, the instantiation because it is
// an instantiation) left templates entirely unchecked.

static void use(in int) noexcept {}
static void fill(out int v) noexcept { v = 1; }
static void consume(move int v) noexcept { (void)v; }

// A read before construction is diagnosed in the instantiated body.
template <class T> static void read_first(out int v) noexcept {
  use(v); // expected-error {{out parameter 'v' is read before it is constructed}}
  v = 1;
  (void)sizeof(T);
}
static void d1() noexcept {
  int z;
  read_first<int>(z); // expected-note {{in instantiation of function template specialization 'read_first<int>' requested here}}
}

// Passing to an out parameter *constructs* it; other parameter kinds read it.
template <class T> static void out_arg(out int v) noexcept {
  fill(v); // out argument: construction, not a read
  use(v);  // OK: constructed above
  (void)sizeof(T);
}
static void d2() noexcept {
  int z;
  out_arg<int>(z);
}

// A parameter never constructed on any path is diagnosed.
template <class T> static void never(out int v) noexcept { // expected-error {{out parameter 'v' is never constructed}}
  (void)sizeof(T);
}
static void d3() noexcept {
  int z;
  never<int>(z); // expected-note {{in instantiation of function template specialization 'never<int>' requested here}}
}

// So is a use after a move.
template <class T> static void moved(int v) noexcept {
  consume(destructive_move(v));
  use(v); // expected-error {{value 'v' is used after it was passed to a move parameter}}
  (void)sizeof(T);
}
static void d4() noexcept {
  moved<int>(1); // expected-note {{in instantiation of function template specialization 'moved<int>' requested here}}
}

// A member of a class template is checked on its instantiation too.
template <class T> struct Holder {
  void member(out int v) noexcept {
    use(v); // expected-error {{out parameter 'v' is read before it is constructed}}
    v = 1;
  }
};
static void d5() noexcept {
  Holder<int> h;
  int z;
  h.member(z); // expected-note {{in instantiation of member function 'Holder<int>::member' requested here}}
}

// A template that is never instantiated produces no code and no diagnostic.
template <class T> static void unused(out int v) noexcept {
  use(v);
}
