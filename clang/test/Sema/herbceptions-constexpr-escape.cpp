// RUN: %clang_cc1 -std=c++26 -fherbceptions -fsyntax-only -verify %s

// A call to a 'throws'/'return_failure{E}' function in a constant-expression
// context (constexpr/constinit variable initializers, static_assert, ...) is
// exempt from the non-throws-caller check: like 'operator new' in a constant
// expression, it is evaluated at compile time and only an error escaping the
// evaluation is diagnosed.

namespace std {

struct error_domain_singleton {
  void (*do_cleanup)(unsigned long) noexcept = 0;
  bool (*do_equivalent)(unsigned long, error_domain_singleton const*, unsigned long) noexcept = 0;
  void (*do_name)(unsigned long, int, void*, void*) noexcept = 0;
  void (*do_message)(unsigned long, int, void*, void*) noexcept = 0;
  int (*do_to_errc)(unsigned long) noexcept = 0;
};

class error {
public:
  error() = delete;
  error(error const&) = delete;
  error(error&&) = delete;
  error& operator=(error const&) = delete;
  error& operator=(error&&) = delete;
  constexpr ~error() noexcept {
    auto docleanup{domain_opaque->do_cleanup};
    if (docleanup) docleanup(code_opaque);
  }
  constexpr error_domain_singleton const* domain() const noexcept { return domain_opaque; }
  constexpr __SIZE_TYPE__ code() const noexcept { return code_opaque; }
private:
  error_domain_singleton const* domain_opaque{};
  __SIZE_TYPE__ code_opaque{};
  explicit constexpr error(void const* domain, __SIZE_TYPE__ code) noexcept
      : domain_opaque(static_cast<error_domain_singleton const*>(domain)), code_opaque(code) {}
  friend constexpr error __builtin_herbception_error(void const*, unsigned long);
};

template<typename T>
struct error_domain;

enum class win32_errc : unsigned { success = 0, invalid_function = 1, file_not_found = 2 };

namespace {
constinit error_domain_singleton dummy_domain{};
}

template<>
struct error_domain<win32_errc> {
  using errc_type = win32_errc;
  static constexpr error_domain_singleton const* domain() noexcept { return &dummy_domain; }
  static constexpr __SIZE_TYPE__ code(errc_type e) noexcept {
    return static_cast<unsigned long>(e);
  }
};

template<typename T>
constexpr bool operator==(error const& e, T t) noexcept {
  return error_domain<T>::code(t) == e.code() &&
         error_domain<T>::domain() == e.domain();
}

} // namespace std

constexpr int tf(int x) throws {
  if (x == 0)
    throw throws ::std::win32_errc::file_not_found; // expected-note 2 {{herbception thrown during constant evaluation was not caught}}
  return 2 * x;
}

constexpr int rf(int x) return_failure{int} {
  if (x == 0)
    return_failure(7); // expected-note {{failure returned during constant evaluation was not caught}}
  return 2 * x;
}

constexpr int caught(int x) {
  try { return tf(x); }
  catch throws(::std::error) { return -1; }
}

// A 'throws' call in a constexpr/constinit initializer inside a non-'throws'
// function is allowed: the initializer is always evaluated at compile time.
void non_throws() {
  constexpr int a{tf(3)}; // OK, no error is thrown
  constexpr int b{rf(3)}; // OK, no failure is returned
  constexpr int c{caught(0)}; // OK, error is caught inside the evaluation
  static_assert(a == 6 && b == 6 && c == -1);

  constexpr int bad_tf{tf(0)}; // expected-error {{constexpr variable 'bad_tf' must be initialized by a constant expression}}
  constexpr int bad_rf{rf(0)}; // expected-error {{constexpr variable 'bad_rf' must be initialized by a constant expression}}

  // A 'throws' call in a non-constant initializer still requires a 'throws'
  // spec or an enclosing 'try { } catch throws' block.
  int runtime = tf(0); // expected-error {{call to 'throws' function in a non-'throws' function}}
}

// static_assert is a constant-evaluated context too.
void static_assert_ctx() {
  static_assert(tf(2) == 4);
  static_assert(rf(2) == 4);
  static_assert(tf(0) == 4); // expected-error {{static assertion expression is not an integral constant expression}} \
                               expected-note {{subexpression not valid in a constant expression}}
}

int main() { non_throws(); static_assert_ctx(); }
