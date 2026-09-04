// RUN: %clang -std=c++20 -fherbceptions -fno-exceptions -S -emit-llvm -o - %s | FileCheck %s

namespace std {
struct error {
  void *domain;
  __SIZE_TYPE__ code;
};
struct error_domain_singleton {};
struct typed_error {
  __SIZE_TYPE__ value;
};
template <class T> class error_domain;
template <> class error_domain<typed_error> {
public:
  static error_domain_singleton const *domain() noexcept;
  static __SIZE_TYPE__ code(typed_error) noexcept;
};
}

struct conversion {
  bool fail;
  operator int() throws {
    if (fail)
      throw throws std::error{nullptr, 71};
    return 9;
  }
};

// CHECK-LABEL: define dso_local {{.*}} @_Z14via_conversion10conversion
// CHECK-COUNT-1: call {{.*}} @_ZN10conversioncviEv
// CHECK: extractvalue {{.*}}, 1
// CHECK: br i1 {{.*}}, label %herb.implicit.err, label %herb.implicit.ok
// CHECK: herb.implicit.err:
// CHECK: store i1 true, ptr %herbception.disc
// CHECK: br label %return
int via_conversion(conversion value) throws {
  return value;
}

struct operand {
  bool fail;
};

int operator+(operand lhs, operand) throws {
  if (lhs.fail)
    throw throws std::error{nullptr, 72};
  return 10;
}

// CHECK-LABEL: define dso_local {{.*}} @_Z12via_operator7operandS_
// CHECK-COUNT-1: call {{.*}} @_Zpl7operandS_
// CHECK: extractvalue {{.*}}, 1
// CHECK: br i1 {{.*}}, label %try.err, label %try.ok
int via_operator(operand lhs, operand rhs) throws {
  return lhs + rhs;
}

int outer_call(int value) throws { return value + 1; }

// The wrapper for the explicit outer call must suppress only that call. The
// implicit conversion used to form its argument has its own discriminator and
// must return before outer_call is reached on failure.
// CHECK-LABEL: define dso_local {{.*}} @_Z17nested_conversion10conversion
// CHECK-COUNT-1: call {{.*}} @_ZN10conversioncviEv
// CHECK: br i1 {{.*}}, label %herb.implicit.err, label %herb.implicit.ok
// CHECK: herb.implicit.ok:
// CHECK-COUNT-1: call {{.*}} @_Z10outer_calli
// CHECK: br i1 {{.*}}, label %try.err, label %try.ok
int nested_conversion(conversion value) throws {
  return outer_call(value);
}

int operator-(operand, operand) return_failure{std::typed_error} {
  return_failure std::typed_error{73};
}

// CHECK-LABEL: define dso_local {{.*}} @_Z18via_typed_operator7operandS_
// CHECK-COUNT-1: call {{.*}} @_Zmi7operandS_
// CHECK: extractvalue {{.*}}, 1
// CHECK: br i1 {{.*}}, label %try.err, label %try.ok
// CHECK: try.err:
// CHECK: call {{.*}}ptr @_ZNSt12error_domainISt11typed_errorE6domainEv
// CHECK: call {{.*}}i64 @_ZNSt12error_domainISt11typed_errorE4codeES0_
// CHECK: store i1 true, ptr %herbception.disc
int via_typed_operator(operand lhs, operand rhs) throws {
  return lhs - rhs;
}

int operator~(operand) return_failure{std::typed_error} {
  return_failure std::typed_error{75};
}

// CHECK-LABEL: define dso_local {{.*}} @_Z15via_typed_unary7operand
// CHECK-COUNT-1: call {{.*}} @_Zco7operand
// CHECK: extractvalue {{.*}}, 1
// CHECK: br i1 {{.*}}, label %try.err, label %try.ok
// CHECK: try.err:
// CHECK: call {{.*}}ptr @_ZNSt12error_domainISt11typed_errorE6domainEv
// CHECK: call {{.*}}i64 @_ZNSt12error_domainISt11typed_errorE4codeES0_
// CHECK: store i1 true, ptr %herbception.disc
int via_typed_unary(operand value) throws {
  return ~value;
}

int cleanup_count;
struct guard {
  ~guard() { ++cleanup_count; }
};

// CHECK-LABEL: define dso_local {{.*}} @_Z12with_cleanup10conversion
// CHECK: herb.implicit.err:
// CHECK: call void @_ZN5guardD2Ev
int with_cleanup(conversion value) throws {
  guard lifetime;
  return value;
}

struct callable {
  int calls;
  int operator()() throws {
    ++calls;
    return 11;
  }
};

// CHECK-LABEL: define dso_local {{.*}} @_Z17via_call_operator8callable
// CHECK-COUNT-1: call {{.*}} @_ZN8callableclEv
int via_call_operator(callable fn) throws {
  return fn();
}

struct constructed {
  bool good;
  constructed(bool fail) throws : good(!fail) {
    if (fail)
      throw throws std::error{nullptr, 74};
  }
};

// Constructors have a source-level void result, but an active effect still
// gives their ABI call a shaped return. The common call path must inspect and
// propagate that discriminator before subsequent initialization is emitted.
// CHECK-LABEL: define dso_local {{.*}} @_Z15via_constructorb
// CHECK-COUNT-1: call {{.*}} @_ZN11constructedC2Eb
// CHECK: extractvalue {{.*}}, 1
// CHECK: br i1 {{.*}}, label %herb.implicit.err, label %herb.implicit.ok
// CHECK: herb.implicit.err:
// CHECK: store i1 true, ptr %herbception.disc
// CHECK: br label %return
int via_constructor(bool fail) throws {
  constructed value(fail);
  return value.good;
}

struct ordinary_pair {
  int value;
  bool flag;
};
ordinary_pair make_ordinary_pair();

// A source-level `{int, bool}` result is an ordinary value and must never enter
// deterministic-failure routing. On x86-64 this particular aggregate is
// coerced to i64, so the test is a semantic negative rather than direct
// coverage of a target that happens to lower an ordinary call as `{T, i1}`;
// the CallInfo effect bit, not physical shape, enforces that invariant.
// CHECK-LABEL: define dso_local {{.*}} @_Z22ordinary_pair_in_catchv
// CHECK: call {{.*}} @_Z18make_ordinary_pairv
// CHECK-NOT: herb.catch
// CHECK-NOT: herb.main
// CHECK: ret i32
int ordinary_pair_in_catch() noexcept {
  try {
    return make_ordinary_pair().value;
  } catch throws(std::error) {
    return -1;
  }
}
