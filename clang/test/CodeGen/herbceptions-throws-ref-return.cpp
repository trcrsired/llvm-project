// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++26 -fherbceptions \
// RUN:     -fno-rtti -emit-llvm %s -o - 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-LREF --check-prefix=CHECK-RREF --check-prefix=CHECK-CREF

// Verify that T& / T&& returns through `throws` preserve the underlying
// value-kind so the caller's `return inner_throws();` does not collapse
// the reference into a temporary. The throws ABI stores the success
// pointer in the {ptr, i1} slot; the caller's auto-propagation must hand
// through the original pointer without copying the referent.

namespace std {
struct error { void *d; __SIZE_TYPE__ c; ~error() noexcept; };
}

int g = 42;

int& lref_inner() { return g; }
int& lref_middle() throws { return lref_inner(); }
int& lref_outer() throws { return lref_middle(); }

int&& rref_inner() { return static_cast<int&&>(g); }
int&& rref_middle() throws { return rref_inner(); }
int&& rref_outer() throws { return rref_middle(); }

const int& cref_inner() { return g; }
const int& cref_middle() throws { return cref_inner(); }
const int& cref_outer() throws { return cref_middle(); }

int main() {
    int& a = lref_outer();
    int&& b = rref_outer();
    const int& c = cref_outer();
    return a + b + c == 126 ? 0 : 1;
}

// Each throws function returns a {{ptr, i64}, i1} slot (not a wrapped T):
//   T&   throws  -> {{ ptr, i64 }, i1 }  (success: first 8 bytes = T*,
//                                         trailing 8 bytes zero;
//                                         error: full error struct in
//                                         the first 16 bytes)
//   T&&  throws  -> same
//   T const& throws  -> same
//
// The slot is sized to max(sizeof(T*), sizeof(std::error)) = 16 bytes
// for the first element so the error can be carried in-place. This
// avoids the bitcast {i64, i64} -> {ptr, i64} bug and preserves
// reference identity.

// CHECK-LREF: define {{[^@]+}}@_Z11lref_middlev() #1
// CHECK-LREF: define {{[^@]+}}@_Z10lref_outerv() #1
// CHECK-RREF: define {{[^@]+}}@_Z11rref_middlev() #1
// CHECK-RREF: define {{[^@]+}}@_Z10rref_outerv() #1
// CHECK-CREF: define {{[^@]+}}@_Z11cref_middlev() #1
// CHECK-CREF: define {{[^@]+}}@_Z10cref_outerv() #1
