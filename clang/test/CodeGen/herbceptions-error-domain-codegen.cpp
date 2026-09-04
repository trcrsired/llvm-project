// RUN: %clang -std=c++20 --target=x86_64-unknown-linux-gnu \
// RUN:   -fherbceptions -fno-exceptions -S -emit-llvm -O0 -o - %s \
// RUN:   | FileCheck %s

// The std::error ABI field is pointer-sized, but an error_domain is permitted
// to return a narrower signed code. Both direct fabrication and typed-failure
// propagation must preserve the declared AST signedness when widening it.
// Domain pointers may likewise originate in a target-specific address space;
// lowering must use an address-space cast rather than an ordinary bitcast.

namespace std {
struct error_domain_singleton {
  int value;
};
struct error {
  error_domain_singleton const *domain;
  __SIZE_TYPE__ code;
};
template <class T> class error_domain;

inline error_domain_singleton signed_domain{};
inline error_domain_singleton address_domain
    __attribute__((address_space(1))) = {1};
} // namespace std

struct signed_error {
  int value;
};

template <> class std::error_domain<signed_error> {
public:
  static std::error_domain_singleton const *domain() noexcept {
    return &std::signed_domain;
  }
  static int code(signed_error value) noexcept { return value.value; }
};

struct address_error {
  int value;
};

template <> class std::error_domain<address_error> {
public:
  static std::error_domain_singleton const __attribute__((address_space(1))) *
  domain() noexcept {
    return &std::address_domain;
  }
  static __SIZE_TYPE__ code(address_error value) noexcept {
    return static_cast<__SIZE_TYPE__>(value.value);
  }
};

enum class narrow_code : signed char { negative = -3 };
struct enum_error {};

template <> class std::error_domain<enum_error> {
public:
  static std::error_domain_singleton const *domain() noexcept {
    return &std::signed_domain;
  }
  static narrow_code code(enum_error) noexcept {
    return narrow_code::negative;
  }
};

extern "C" __attribute__((noinline)) int signed_source()
    return_failure{signed_error} {
  return_failure signed_error{-7};
}

extern "C" __attribute__((noinline)) int address_source()
    return_failure{address_error} {
  return_failure address_error{9};
}

extern "C" __attribute__((noinline)) int enum_source()
    return_failure{enum_error} {
  return_failure enum_error{};
}

extern "C" __attribute__((noinline)) void direct_signed() throws {
  throw throws signed_error{-7};
}

// CHECK-LABEL: define{{.*}} @direct_signed(
// CHECK: [[DIRECT_CODE:%.*]] = call{{.*}} i32
// CHECK: [[DIRECT_WIDE:%.*]] = sext i32 [[DIRECT_CODE]] to i64

extern "C" __attribute__((noinline)) int propagate_signed() throws {
  return signed_source();
}

// CHECK-LABEL: define{{.*}} @propagate_signed(
// CHECK: [[TYPED_CODE:%.*]] = call{{.*}} i32
// CHECK: [[TYPED_WIDE:%.*]] = sext i32 [[TYPED_CODE]] to i64

template <bool Live> __attribute__((noinline)) int dependent_signed() throws {
  if constexpr (Live)
    return signed_source();
  return 0;
}

template int dependent_signed<false>();
template int dependent_signed<true>();

// A non-dependent call in a dependent constexpr arm can otherwise be reused
// verbatim by TreeTransform, leaving its deferred CXXTryExpr without accessor
// metadata when the arm becomes live after substitution.
// CHECK-LABEL: define{{.*}} @_Z16dependent_signedILb1EEiv(
// CHECK: [[DEPENDENT_CODE:%.*]] = call{{.*}} i32
// CHECK: [[DEPENDENT_WIDE:%.*]] = sext i32 [[DEPENDENT_CODE]] to i64

extern "C" __attribute__((noinline)) void direct_address() throws {
  throw throws address_error{9};
}

// CHECK-LABEL: define{{.*}} @direct_address(
// CHECK: [[DIRECT_DOMAIN:%.*]] = call{{.*}} ptr addrspace(1)
// CHECK: [[DIRECT_CAST:%.*]] = addrspacecast ptr addrspace(1) [[DIRECT_DOMAIN]] to ptr

extern "C" __attribute__((noinline)) int propagate_address() throws {
  return address_source();
}

// CHECK-LABEL: define{{.*}} @propagate_address(
// CHECK: [[TYPED_DOMAIN:%.*]] = call{{.*}} ptr addrspace(1)
// CHECK: [[TYPED_CAST:%.*]] = addrspacecast ptr addrspace(1) [[TYPED_DOMAIN]] to ptr

extern "C" __attribute__((noinline)) int propagate_enum() throws {
  return enum_source();
}

// The enum's signed underlying type, rather than the fact that the accessor's
// source-level return type is an enum, controls widening into size_t.
// CHECK-LABEL: define{{.*}} @propagate_enum(
// CHECK: [[ENUM_CODE:%.*]] = call{{.*}} i8
// CHECK: [[ENUM_WIDE:%.*]] = sext i8 [[ENUM_CODE]] to i64
