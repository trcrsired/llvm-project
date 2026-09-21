// RUN: %clang_cc1 -std=c++26 -fherbceptions -fsyntax-only -verify %s

// A bare call to a 'throws'/'return_failure{E}' function inside a function
// that cannot propagate a herbception is diagnosed in Sema, so it is caught
// by -fsyntax-only rather than only at CodeGen. The contexts CodeGen can
// still route (try bodies, catch clauses) or that trap (main) are deferred.

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
  __SIZE_TYPE__ code() const noexcept { return code_opaque; }

private:
  void const *domain_opaque{};
  __SIZE_TYPE__ code_opaque{};
  explicit constexpr error(void const *domain, __SIZE_TYPE__ code) noexcept
      : domain_opaque(domain), code_opaque(code) {}
};
} // namespace std

void throwing() throws;
int throwing_ret() throws;

// throws caller: the call auto-propagates.
void ok_throws() throws { throwing(); }

// main(): an escaped error traps at runtime.
int main() { throwing(); }

void bad_noexcept() noexcept {
  throwing(); // expected-error {{call to 'throws' function in a non-'throws' function must be handled by an enclosing 'try { } catch throws' block, or mark the calling function as 'throws' so herbceptions can propagate}}
}

void bad_plain() {
  throwing(); // expected-error {{call to 'throws' function in a non-'throws' function must be handled by an enclosing 'try { } catch throws' block, or mark the calling function as 'throws' so herbceptions can propagate}}
}

// A try body defers: the sibling `catch throws` handler consumes the error.
void ok_try_block() noexcept {
  try {
    throwing();
  } catch throws(::std::error e) {
    (void)e;
  }
}

// A possibly-discarded 'if constexpr' branch defers.
void ok_discarded() noexcept {
  if constexpr (sizeof(int) > 100) {
    throwing();
  }
}

// Unevaluated operands are never emitted.
static_assert(noexcept(throwing_ret()) || !noexcept(throwing_ret()));

// A default argument is evaluated in the caller's frame: a throws caller
// may use it even though the declared function is noexcept.
void with_default(int = throwing_ret()) noexcept;
void ok_caller() throws { with_default(); }

// Lambdas have their own spec.
void bad_lambda() noexcept {
  []() noexcept {
    throwing(); // expected-error {{call to 'throws' function in a non-'throws' function must be handled by an enclosing 'try { } catch throws' block, or mark the calling function as 'throws' so herbceptions can propagate}}
  }();
}

void ok_lambda() throws {
  []() throws { throwing(); }();
}
