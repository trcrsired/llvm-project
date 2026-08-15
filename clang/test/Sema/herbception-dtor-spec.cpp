// RUN: %clang_cc1 -std=c++26 -fherbceptions -fcxx-exceptions -fsyntax-only -verify %s

// Destructors cannot be declared with a herbception exception specification:
// destruction must be able to run during cleanup, so it cannot itself fail
// through the throws/fails channel.

struct DtorThrows {
  ~DtorThrows() throws; // expected-error {{destructor cannot be declared with a herbception ('throws'/'fails{...}') exception specification; destructors cannot propagate errors}}
};

struct DtorThrowsTrue {
  ~DtorThrowsTrue() throws(true); // expected-error {{destructor cannot be declared with a herbception ('throws'/'fails{...}') exception specification; destructors cannot propagate errors}}
};

struct DtorFails {
  ~DtorFails() fails{int}; // expected-error {{destructor cannot be declared with a herbception ('throws'/'fails{...}') exception specification; destructors cannot propagate errors}}
};

// noexcept destructors remain fine.
struct DtorOk {
  ~DtorOk() noexcept;
};
