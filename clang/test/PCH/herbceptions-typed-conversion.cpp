// RUN: %clang_cc1 -std=c++20 -fherbceptions -emit-pch -o %t %s
// RUN: %clang_cc1 -std=c++20 -fherbceptions -include-pch %t \
// RUN:   -emit-llvm -verify -o - %s | FileCheck %s

#ifndef HERBCEPTIONS_TYPED_CONVERSION_HEADER
#define HERBCEPTIONS_TYPED_CONVERSION_HEADER

namespace std {
struct error_domain_singleton {};
struct error {
  error_domain_singleton const *domain;
  __SIZE_TYPE__ code;
};
template <class T> class error_domain;

struct typed_error {
  int value;
};
inline error_domain_singleton serialized_domain;
template <> class error_domain<typed_error> {
public:
  static error_domain_singleton const *domain() noexcept {
    return &serialized_domain;
  }
  static __SIZE_TYPE__ code(typed_error value) noexcept {
    return static_cast<__SIZE_TYPE__>(value.value);
  }
};
} // namespace std

struct serialized_value {
  int value;
};

serialized_value serialized_source(bool) return_failure{std::typed_error};

inline int serialized_propagation(bool fail) throws {
  // This CXXTryExpr is emitted only by the translation unit loading the PCH.
  // Its saved E, domain declaration, and conversion-target bit must therefore
  // all survive AST serialization rather than being rediscovered from LLVM's
  // payload representative.
  return serialized_source(fail).value;
}

#else

serialized_value serialized_source(bool fail) return_failure{std::typed_error} {
  if (fail)
    return_failure std::typed_error{97};
  return serialized_value{23};
}

int instantiate_serialized_propagation(bool fail) throws {
  return serialized_propagation(fail);
}

// The accessor declarations, semantic E, and propagation-conversion bit are
// serialized fields of CXXTryExpr. Checking the emitted calls ensures this is
// not merely a no-crash PCH test that could silently raw-copy typed_error.
// CHECK-LABEL: define linkonce_odr {{.*}} @_Z22serialized_propagationb
// CHECK: call {{.*}} @_ZNSt12error_domainISt11typed_errorE6domainEv
// CHECK: call {{.*}} @_ZNSt12error_domainISt11typed_errorE4codeES0_

// expected-no-diagnostics

#endif
