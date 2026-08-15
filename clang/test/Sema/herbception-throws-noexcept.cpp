// RUN: %clang_cc1 -std=c++26 -fherbceptions -fcxx-exceptions -fsyntax-only -verify %s

// A 'throws' function implies noexcept(true), so it cannot be combined with
// 'noexcept(false)' in either order. 'throws noexcept(true)' and
// 'noexcept(true) throws' are consistent (throws implies noexcept(true)) and
// remain 'throws' functions.

void a() throws noexcept(false); // expected-error {{'throws' and 'noexcept(false)' cannot be combined: a 'throws' function implies noexcept(true)}}
void b() noexcept(false) throws; // expected-error {{'throws' and 'noexcept(false)' cannot be combined: a 'throws' function implies noexcept(true)}}
void c() throws(true) noexcept(false); // expected-error {{'throws' and 'noexcept(false)' cannot be combined: a 'throws' function implies noexcept(true)}}
void d() noexcept(false) throws(true); // expected-error {{'throws' and 'noexcept(false)' cannot be combined: a 'throws' function implies noexcept(true)}}

// Consistent combinations stay 'throws'.
void e() throws noexcept(true);
void f() noexcept(true) throws;
void g() noexcept throws;
void h() throws noexcept;

// throws(false) means the function cannot fail: it becomes noexcept.
void i() throws(false);
void j() noexcept(true) throws(false);

// Member functions (delayed exception spec parsing) behave identically.
struct S {
  void a() throws noexcept(false); // expected-error {{'throws' and 'noexcept(false)' cannot be combined: a 'throws' function implies noexcept(true)}}
  void b() noexcept(false) throws; // expected-error {{'throws' and 'noexcept(false)' cannot be combined: a 'throws' function implies noexcept(true)}}
  void c() noexcept(true) throws;
  void d() throws noexcept(true);
  void e() noexcept throws;
};

// Lambdas behave identically.
auto l1 = []() noexcept(true) throws {};
auto l2 = []() throws noexcept {};
auto l3 = []() noexcept(false) throws {}; // expected-error {{'throws' and 'noexcept(false)' cannot be combined: a 'throws' function implies noexcept(true)}}
