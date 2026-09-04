// RUN: %clang_cc1 -std=c++20 -fherbceptions -fsyntax-only \
// RUN:   -verify %s

using size_t = __SIZE_TYPE__;
namespace std {
struct error {
  void *domain;
  size_t code;
};
} // namespace std

// Disabling C++ unwinding must not disable checks for active herbception
// channels: these specifications change the physical return ABI.
struct BareBase {
  virtual void run() throws; // expected-note {{overridden virtual function is here}}
};
struct MissingBare : BareBase {
  void run() override; // expected-error {{overriding function has a different herbceptions ('throws'/'return_failure{...}') specifier than the base version, which changes the calling convention}}
};

struct PlainBase {
  virtual void run(); // expected-note {{overridden virtual function is here}}
};
struct AddedBare : PlainBase {
  void run() throws override; // expected-error {{overriding function has a different herbceptions ('throws'/'return_failure{...}') specifier than the base version, which changes the calling convention}}
};

struct TrueBase {
  virtual void run() throws(true); // expected-note {{overridden virtual function is here}}
};
struct MissingTrue : TrueBase {
  void run() noexcept override; // expected-error {{overriding function has a different herbceptions ('throws'/'return_failure{...}') specifier than the base version, which changes the calling convention}}
};

struct NoexceptBase {
  virtual void run() noexcept; // expected-note {{overridden virtual function is here}}
};
struct AddedTrue : NoexceptBase {
  void run() throws(true) override; // expected-error {{overriding function has a different herbceptions ('throws'/'return_failure{...}') specifier than the base version, which changes the calling convention}}
};

// Bare throws and throws(true) are the same active channel and may override
// each other in either direction.
struct CanonicalBareBase {
  virtual void run() throws;
};
struct CanonicalTrueOverride : CanonicalBareBase {
  void run() throws(true) override;
};
struct CanonicalTrueBase {
  virtual void run() throws(true);
};
struct CanonicalBareOverride : CanonicalTrueBase {
  void run() throws override;
};

// throws(false) is noexcept(true), not an active herbception channel.
struct FalseBase {
  virtual void run() throws(false);
};
struct NoexceptOverride : FalseBase {
  void run() noexcept override;
};
struct NoexceptEquivalentBase {
  virtual void run() noexcept;
};
struct FalseOverride : NoexceptEquivalentBase {
  void run() throws(false) override;
};

// Preserve Clang's historical -fno-exceptions behavior for ordinary C++
// exception specifications: this more-lax override remains accepted.
struct TraditionalNoexceptBase {
  virtual void run() noexcept;
};
struct TraditionalPlainOverride : TraditionalNoexceptBase {
  void run() override;
};

// Typed return_failure is restricted to free functions, but it remains an
// active, ABI-distinct function type when traditional exceptions are off.
using TypedFailure = int() return_failure{int};
using PlainFunction = int();
static_assert(!__is_same(TypedFailure, PlainFunction));
