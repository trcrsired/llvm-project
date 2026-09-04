// RUN: %clang -std=c++20 -O0 -fherbceptions -fno-exceptions %s -o %t.o0
// RUN: %t.o0
// RUN: %clang -std=c++20 -O2 -fherbceptions -fno-exceptions %s -o %t.o2
// RUN: %t.o2
// REQUIRES: native

// Verify that a typed failure is decoded from the union carrier as its
// semantic E, not as the unrelated success T selected by LLVM. The aligned
// reference parameter also catches temporaries whose IR alignment was derived
// from the carrier representative instead of the AST type.

namespace std {
struct error_domain_singleton {};

inline int error_destructors;

struct error {
  error_domain_singleton const *domain_value;
  __SIZE_TYPE__ code_value;
  ~error() noexcept { ++error_destructors; }
};

template <class T> class error_domain;

struct alignas(32) typed_error {
  __SIZE_TYPE__ code;
  unsigned char padding[24];
};

inline error_domain_singleton typed_domain;
inline int domain_calls;
inline int code_calls;
inline int aligned_arguments;

template <> class error_domain<typed_error> {
public:
  static error_domain_singleton const *domain() noexcept {
    ++domain_calls;
    return &typed_domain;
  }

  static __SIZE_TYPE__ code(typed_error const &value) noexcept {
    ++code_calls;
    aligned_arguments +=
        reinterpret_cast<__UINTPTR_TYPE__>(&value) % alignof(typed_error) == 0;
    return value.code;
  }
};

enum class scalar_error : int { failed = 93 };
inline error_domain_singleton scalar_domain;
inline int scalar_code_calls;

template <> class error_domain<scalar_error> {
public:
  static error_domain_singleton const *domain() noexcept {
    return &scalar_domain;
  }
  static __SIZE_TYPE__ code(scalar_error value) noexcept {
    ++scalar_code_calls;
    return static_cast<__SIZE_TYPE__>(value);
  }
};

struct inherited_error {
  int value;
};
inline error_domain_singleton inherited_domain;
inline int inherited_code_calls;

template <class Error> class inherited_error_domain_base {
public:
  static error_domain_singleton const *domain() noexcept {
    return &inherited_domain;
  }
  static int code(Error) noexcept {
    ++inherited_code_calls;
    return -1;
  }
  static int code(void *) noexcept { return 0; }
};

// Qualified lookup must retain inherited accessors and overload resolution
// must select code(inherited_error), rather than letting CodeGen rescan only
// the specialization's direct declarations.
template <> class error_domain<inherited_error>
    : public inherited_error_domain_base<inherited_error> {};

inline char pointer_payload[17];
inline error_domain_singleton pointer_domain;
inline int pointer_code_calls;

template <> class error_domain<char *> {
public:
  static error_domain_singleton const *domain() noexcept {
    return &pointer_domain;
  }

  static __SIZE_TYPE__
  code(char *const value __attribute__((pass_object_size(0)))) noexcept {
    ++pointer_code_calls;
    // The OpaqueValueExpr has no source object-size provenance, but ordinary
    // call emission must still supply the ABI-mandated hidden size argument.
    __SIZE_TYPE__ size = __builtin_object_size(value, 0);
    return size == static_cast<__SIZE_TYPE__>(-1) ? 103 : size;
  }
};

struct rvalue_error {
  int value;
};
inline error_domain_singleton rvalue_domain;
inline int rvalue_code_calls;

template <> class error_domain<rvalue_error> {
public:
  static error_domain_singleton const *domain() noexcept {
    return &rvalue_domain;
  }
  static int code(rvalue_error &&value) noexcept {
    ++rvalue_code_calls;
    return value.value;
  }
};
} // namespace std

struct cxx_std_error {
  void const *domain;
  __SIZE_TYPE__ code;
};

inline int success_destructors;

struct success_value {
  int value;
  ~success_value() { ++success_destructors; }
};

__attribute__((noinline)) success_value
make_value(bool fail) return_failure{std::typed_error} {
  if (fail)
    return_failure std::typed_error{91, {}};
  return success_value{17};
}

// T and E have the same storage size but different LLVM types. Before this
// regression was fixed, the carrier was stored as %success_value and then
// loaded as the two-word std::error, reading beyond the temporary.
__attribute__((noinline)) success_value
make_scalar_value(bool fail) return_failure{std::scalar_error} {
  if (fail)
    return_failure std::scalar_error::failed;
  return success_value{19};
}

__attribute__((noinline)) int nested_inner(bool fail)
    return_failure{std::scalar_error} {
  if (fail)
    return_failure std::scalar_error::failed;
  return 31;
}

struct initialized_from_typed {
  int value;
  explicit initialized_from_typed(bool fail) throws
      : value(nested_inner(fail)) {}
};

__attribute__((noinline)) int propagate_initializer(bool fail) throws {
  initialized_from_typed object(fail);
  return object.value;
}

inline int function_try_prefix_live;
inline int function_try_prefix_destructors;
inline int function_try_handler_calls;
inline __SIZE_TYPE__ function_try_handler_code;

struct function_try_prefix {
  function_try_prefix() { ++function_try_prefix_live; }
  ~function_try_prefix() {
    --function_try_prefix_live;
    ++function_try_prefix_destructors;
  }
};

struct function_try_initialized {
  function_try_prefix prefix;
  int value;

  explicit function_try_initialized(bool fail) throws try
      : prefix(), value(nested_inner(fail)) {
  } catch throws(std::error error) {
    ++function_try_handler_calls;
    function_try_handler_code = error.code_value;
    // Constructor function-try fallthrough must re-propagate this exact error;
    // it cannot report success for a partially constructed object.
  }
};

__attribute__((noinline)) int
propagate_function_try_initializer(bool fail) throws {
  function_try_initialized object(fail);
  return object.value;
}

inline int nested_outer_calls;
__attribute__((noinline)) int nested_outer(int value) throws {
  ++nested_outer_calls;
  return value + 1;
}

// The explicit wrapper owns nested_outer only. nested_inner must still gain
// its own CXXTryExpr so its typed error is converted before outer evaluation.
__attribute__((noinline)) int explicit_nested(bool fail) throws {
  return try(nested_outer(nested_inner(fail)));
}

struct outer_operand {};
inline int outer_operator_calls;
__attribute__((noinline)) int operator+(outer_operand, int value) throws {
  ++outer_operator_calls;
  return value + 2;
}

// Operator notation takes the direct SemaOverload path; it must obey the same
// top-level idempotence and preserve the nested typed conversion.
__attribute__((noinline)) int explicit_nested_operator(bool fail) throws {
  return try(outer_operand{} + nested_inner(fail));
}

__attribute__((noinline)) int inherited_failure()
    return_failure{std::inherited_error} {
  return_failure std::inherited_error{1};
}

__attribute__((noinline)) int propagate_inherited() throws {
  return inherited_failure();
}

__attribute__((noinline)) void throw_inherited_directly() throws {
  // The direct `throw throws` fabrication path shares the same signed code
  // conversion contract as typed-failure propagation.
  throw throws std::inherited_error{2};
}

__attribute__((noinline)) int propagate(bool fail) throws {
  success_value value = make_value(fail);
  return value.value;
}

__attribute__((noinline)) int pointer_failure(bool fail)
    return_failure{char *} {
  if (fail)
    return_failure std::pointer_payload;
  return 29;
}

__attribute__((noinline)) int propagate_pointer(bool fail) throws {
  return pointer_failure(fail);
}

__attribute__((noinline)) int rvalue_failure(bool fail)
    return_failure{std::rvalue_error} {
  if (fail)
    return_failure std::rvalue_error{107};
  return 31;
}

__attribute__((noinline)) int propagate_rvalue(bool fail) throws {
  return rvalue_failure(fail);
}

__attribute__((noinline)) int c_bridge_failure(bool fail)
    return_failure{cxx_std_error} {
  if (fail)
    return_failure cxx_std_error{&std::typed_domain, 109};
  return 37;
}

__attribute__((noinline)) int propagate_c_bridge(bool fail) throws {
  return c_bridge_failure(fail);
}

__attribute__((noinline)) int enter_handler() throws {
  throw throws std::scalar_error::failed;
}

__attribute__((noinline)) int explicit_bare_rethrow() throws {
  try {
    return enter_handler();
  } catch throws(std::error) {
    throw throws;
  }
}

__attribute__((noinline)) int handler_body_raw_propagation()
    return_failure{std::scalar_error} {
  try {
    return enter_handler();
  } catch throws(std::error) {
    // The handler's std::error slot has been popped. This failure must remain
    // raw and flow through the enclosing typed function channel.
    return nested_inner(true);
  }
}

int main() {
  int caught = 0;

  // An ordinary function has no return-channel LLVM error type. The nearest
  // catch-throws handler is therefore the sole conversion destination.
  try {
    success_value value = make_value(true);
    (void)value;
    return 1;
  } catch throws(std::error error) {
    caught += error.domain_value == &std::typed_domain &&
              error.code_value == 91;
  }

  // A basic throws caller converts E while forwarding through its own shaped
  // return before the outer handler binds std::error.
  try {
    (void)propagate(true);
    return 2;
  } catch throws(std::error error) {
    caught += error.domain_value == &std::typed_domain &&
              error.code_value == 91;
  }

  try {
    if (propagate(false) != 17)
      return 3;
  } catch throws(std::error) {
    return 4;
  }


  try {
    success_value value = make_scalar_value(true);
    (void)value;
    return 5;
  } catch throws(std::error error) {
    caught += error.domain_value == &std::scalar_domain &&
              error.code_value == 93;
  }

  try {
    (void)explicit_nested(true);
    return 7;
  } catch throws(std::error error) {
    caught += error.domain_value == &std::scalar_domain &&
              error.code_value == 93 && nested_outer_calls == 0;
  }

  try {
    if (explicit_nested(false) != 32 || nested_outer_calls != 1)
      return 8;
  } catch throws(std::error) {
    return 9;
  }

  try {
    (void)explicit_nested_operator(true);
    return 10;
  } catch throws(std::error error) {
    caught += error.domain_value == &std::scalar_domain &&
              error.code_value == 93 && outer_operator_calls == 0;
  }

  try {
    if (explicit_nested_operator(false) != 33 || outer_operator_calls != 1)
      return 11;
  } catch throws(std::error) {
    return 12;
  }

  try {
    (void)propagate_inherited();
    return 13;
  } catch throws(std::error error) {
    // The selected code() returns signed int -1. Conversion to size_t must
    // sign-extend before the usual modulo interpretation on wider targets.
    caught += error.domain_value == &std::inherited_domain &&
              error.code_value == static_cast<__SIZE_TYPE__>(-1);
  }

  try {
    throw_inherited_directly();
    return 14;
  } catch throws(std::error error) {
    caught += error.domain_value == &std::inherited_domain &&
              error.code_value == static_cast<__SIZE_TYPE__>(-1);
  }

  try {
    (void)propagate_pointer(true);
    return 15;
  } catch throws(std::error error) {
    caught += error.domain_value == &std::pointer_domain &&
              error.code_value == 103;
  }

  try {
    (void)propagate_rvalue(true);
    return 16;
  } catch throws(std::error error) {
    caught += error.domain_value == &std::rvalue_domain &&
              error.code_value == 107;
  }

  try {
    (void)propagate_c_bridge(true);
    return 17;
  } catch throws(std::error error) {
    caught += error.domain_value == &std::typed_domain &&
              error.code_value == 109;
  }

  auto raw = catch return_failure(handler_body_raw_propagation());
  caught += raw.failed && raw.error == std::scalar_error::failed;

  const int destructors_before_bare_rethrow = std::error_destructors;
  bool bare_rethrow_caught = false;
  try {
    (void)explicit_bare_rethrow();
    return 18;
  } catch throws(std::error error) {
    bare_rethrow_caught =
        error.domain_value == &std::scalar_domain && error.code_value == 93;
  }
  caught += bare_rethrow_caught &&
            std::error_destructors == destructors_before_bare_rethrow + 1;

  try {
    (void)propagate_initializer(true);
    return 19;
  } catch throws(std::error error) {
    caught += error.domain_value == &std::scalar_domain &&
              error.code_value == 93;
  }

  const int destructors_before_function_try = std::error_destructors;
  bool function_try_caught = false;
  try {
    (void)propagate_function_try_initializer(true);
    return 20;
  } catch throws(std::error error) {
    function_try_caught =
        error.domain_value == &std::scalar_domain && error.code_value == 93;
  }
  caught += function_try_caught && function_try_handler_calls == 1 &&
            function_try_handler_code == 93 && function_try_prefix_live == 0 &&
            function_try_prefix_destructors == 1 &&
            std::error_destructors == destructors_before_function_try + 1;

  return caught == 14 && std::domain_calls == 2 && std::code_calls == 2 &&
                 std::aligned_arguments == 2 && success_destructors == 1
                 && std::scalar_code_calls == 7 &&
                 std::inherited_code_calls == 2 &&
                 std::pointer_code_calls == 1 &&
                 std::rvalue_code_calls == 1
             ? 0
             : 6;
}
