// RUN: %clang -std=c++20 -fherbceptions -fno-exceptions %s -o %t
// RUN: %t
// REQUIRES: native

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
  static inline error_domain_singleton singleton;
  static error_domain_singleton const *domain() noexcept {
    return &singleton;
  }
  static __SIZE_TYPE__ code(typed_error error) noexcept {
    return error.value;
  }
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

int via_operator(operand lhs, operand rhs) throws {
  return lhs + rhs;
}

int outer_calls;
int outer_call(int value) throws {
  ++outer_calls;
  return value + 1;
}

int nested_conversion(conversion value) throws {
  return outer_call(value);
}

int operator-(operand, operand) return_failure{std::typed_error} {
  return_failure std::typed_error{73};
}

int via_typed_operator(operand lhs, operand rhs) throws {
  return lhs - rhs;
}

int operator~(operand) return_failure{std::typed_error} {
  return_failure std::typed_error{75};
}

int via_typed_unary(operand value) throws {
  return ~value;
}

int cleanup_count;
struct guard {
  ~guard() { ++cleanup_count; }
};

int with_cleanup(conversion value) throws {
  guard lifetime;
  return value;
}

int constructor_calls;
int destructor_calls;
struct constructed {
  bool good;
  constructed(bool fail) throws : good(!fail) {
    ++constructor_calls;
    if (fail)
      throw throws std::error{nullptr, 74};
  }
  ~constructed() { ++destructor_calls; }
};

int via_constructor(bool fail) throws {
  constructed value(fail);
  return value.good ? 12 : 0;
}

int main() {
  int caught = 0;
  try {
    (void)via_conversion({true});
  } catch throws(std::error error) {
    caught += error.code == 71;
  }
  try {
    (void)via_operator({true}, {false});
  } catch throws(std::error error) {
    caught += error.code == 72;
  }
  try {
    (void)via_typed_operator({false}, {false});
  } catch throws(std::error error) {
    caught += error.code == 73;
  }
  try {
    (void)via_typed_unary({false});
  } catch throws(std::error error) {
    caught += error.code == 75;
  }
  try {
    (void)nested_conversion({true});
  } catch throws(std::error error) {
    caught += error.code == 71 && outer_calls == 0;
  }
  try {
    (void)with_cleanup({true});
  } catch throws(std::error error) {
    caught += error.code == 71;
  }

  int success = 0;
  try {
    success = via_constructor(false);
  } catch throws(std::error) {
    return 2;
  }
  try {
    (void)via_constructor(true);
  } catch throws(std::error error) {
    caught += error.code == 74;
  }
  return caught == 7 && cleanup_count == 1 && success == 12 &&
                 constructor_calls == 2 && destructor_calls == 1
             ? 0
             : 1;
}
