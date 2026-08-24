// RUN: %clang_cc1 -std=c++26 -fherbceptions -fsyntax-only -verify %s
//
// `throw throws expr` is valid inside any try block (including the body of a
// function-try-block), where the error routes to the enclosing catch-throws
// handler. Only inside a `catch throws` handler body itself must the bare
// rethrow form be used.

namespace std {
struct error_domain_singleton {
  void (*do_cleanup)(unsigned long) noexcept = 0;
  bool (*do_equivalent)(unsigned long, error_domain_singleton const *,
                        unsigned long) noexcept = 0;
  void (*do_name)(unsigned long, int, void *, void *) noexcept = 0;
  void (*do_message)(unsigned long, int, void *, void *) noexcept = 0;
  int (*do_to_errc)(unsigned long) noexcept = 0;
};

class error {
public:
  error() = delete;
  error(error const &) = delete;
  error &operator=(error const &) = delete;
  constexpr ~error() noexcept {}
  unsigned long code() const noexcept { return code_opaque; }

private:
  void const *domain_opaque{};
  unsigned long code_opaque{};
  explicit constexpr error(void const *domain, unsigned long code) noexcept
      : domain_opaque(domain), code_opaque(code) {}
};

template <typename T> struct error_domain;

enum class errc : int { invalid_argument = 22, bad_address = 14 };

const error_domain_singleton dummy_domain{};

template <> struct error_domain<errc> {
  static constexpr error_domain_singleton const *domain() noexcept {
    return &dummy_domain;
  }
  static constexpr unsigned long code(errc e) noexcept {
    return static_cast<unsigned long>(e);
  }
};
} // namespace std

// Function-try-block: throwing a new error from the body routes to the local
// handler instead of escaping main.
int main()
try {
  throw throws ::std::errc::invalid_argument;
}
catch throws(::std::error e) {
  (void)e.code();
}

void operand_in_try_body() {
  try {
    throw throws ::std::errc::invalid_argument;
  } catch throws(::std::error e) {
    throw throws; // ok: bare rethrow inside the handler
  }
}

void operand_in_innermost_handler_ok() throws {
  try {
    throw throws ::std::errc::invalid_argument;
  } catch throws(::std::error e) {
    // A new error raised inside a handler leaves via the enclosing
    // function's own throws channel.
    throw throws ::std::errc::bad_address;
  }
}

void operand_in_handler_nowhere_to_go() {
  try {
    throw throws ::std::errc::invalid_argument;
  } catch throws(::std::error e) {
    throw throws ::std::errc::bad_address;
    // expected-error@-1 {{'throw throws' in a plain (non-'throws') function must be inside a 'try { } catch throws' block}}
  }
}

void bare_in_try_body() {
  try {
    throw throws;
    // expected-error@-1 {{bare 'throw throws' (rethrow) is only allowed inside a 'catch throws' block}}
  } catch throws(::std::error e) {
  }
}

int plain_function() {
  throw throws ::std::errc::invalid_argument;
  // expected-error@-1 {{'throw throws' is only allowed inside a function declared 'throws' or 'fails{...}'}}
  return 0;
}
