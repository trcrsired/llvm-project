// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fherbceptions -fclangir -emit-cir %s -o %t.cir 2>&1 | FileCheck %s

// Destructors must NOT have throws - this should be a compile error.

namespace std {
struct error { void *d; unsigned long long c; };
}

struct Bad {
  // Destructor with throws should be rejected
  // CHECK: error: destructor cannot be declared with a herbception ('throws'/'fails{...}') exception specification; destructors cannot propagate errors
  ~Bad() throws {}
};

struct Bad2 {
  // Destructor with throws(cond) where cond is false should also be rejected
  // CHECK: error: destructor cannot be declared with a herbception ('throws'/'fails{...}') exception specification; destructors cannot propagate errors
  ~Bad2() throws(false) {}
};

struct Good {
  // Destructor with noexcept is fine
  ~Good() noexcept = default;
};
