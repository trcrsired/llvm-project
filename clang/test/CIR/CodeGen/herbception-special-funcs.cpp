// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fherbceptions -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fherbceptions -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s -check-prefix=LLVM
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fherbceptions -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s -check-prefix=OGCG

// Herbception special functions: constructors, copy constructors, operator
// overloading, and destructors with throws.

namespace std {
struct error { void *d; unsigned long long c; };
}

// Constructor with throws
struct Foo {
  int x;
  // CIR: cir.func @_ZN3FooC2Ei
  // CIR-SAME: cir.throws
  Foo(int x) throws : x(x) {}
  // CIR: cir.func @_ZN3FooC2EOS_
  // CIR-SAME: cir.throws
  Foo(Foo&& other) throws : x(other.x) {}
  // CIR: cir.func @_ZN3FooC2ERKS_
  // CIR-SAME: cir.throws
  Foo(const Foo& other) throws : x(other.x) {}
  // Destructor must NOT have throws
  ~Foo() noexcept = default;
};

// Operator overloading with throws
struct Bar {
  int val;
  Bar(int v) : val(v) {}
  // CIR: cir.func @_ZN3BarplERKS_
  // CIR-SAME: cir.throws
  Bar operator+(const Bar& other) throws {
    Bar result(val + other.val);
    if (result.val < 0) throw throws std::error{nullptr, 4};
    return result;
  }
  // CIR: cir.func @_ZN3BarixEi
  // CIR-SAME: cir.throws
  int& operator[](int idx) throws {
    if (idx < 0) throw throws std::error{nullptr, 5};
    return val;
  }
};

// CIR: cir.func @_Z10make_foov
// CIR-SAME: cir.throws
Foo make_foo() throws {
  return Foo(42);
}

// CIR: cir.func @_Z10copy_foov
// CIR-SAME: cir.throws
Foo copy_foo(const Foo& f) throws {
  return Foo(f);
}

// LLVM: define{{.*}} @_ZN3FooC2Ei
// LLVM-SAME: throws
// LLVM: define{{.*}} @_ZN3FooC2EOS_
// LLVM-SAME: throws
// LLVM: define{{.*}} @_ZN3FooC2ERKS_
// LLVM-SAME: throws
// LLVM: define{{.*}} @_ZN3BarplERKS_
// LLVM-SAME: throws
// LLVM: define{{.*}} @_ZN3BarixEi
// LLVM-SAME: throws

// OGCG: define{{.*}} @_ZN3FooC2Ei
// OGCG-SAME: throws
// OGCG: define{{.*}} @_ZN3FooC2EOS_
// OGCG-SAME: throws
// OGCG: define{{.*}} @_ZN3FooC2ERKS_
// OGCG-SAME: throws
// OGCG: define{{.*}} @_ZN3BarplERKS_
// OGCG-SAME: throws
// OGCG: define{{.*}} @_ZN3BarixEi
// OGCG-SAME: throws

int main() {
  Foo f(10);
  Foo f2 = f;
  Bar a(1);
  Bar b(2);
  return 0;
}
