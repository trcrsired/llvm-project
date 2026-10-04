// RUN: %clang_cc1 -std=c++26 -fherbceptions -fsyntax-only -verify %s
// expected-no-diagnostics

// std::error::release() transfers ownership of the {domain, code} payload
// out of a caught error and rebinds the error to a success domain. Because
// it binds through error_domain<T>::domain() — which the constant evaluator
// intercepts to fabricate an opaque domain — it is usable in constant
// expressions just like e == errc and e.code() are.

namespace std {

struct error_domain_singleton {
  void (*do_cleanup)(unsigned long) noexcept = 0;
  bool (*do_equivalent)(unsigned long, error_domain_singleton const *, unsigned long) noexcept = 0;
  void (*do_name)(unsigned long, int, void *, void *) noexcept = 0;
  void (*do_message)(unsigned long, int, void *, void *) noexcept = 0;
  int (*do_to_errc)(unsigned long) noexcept = 0;
};

struct cxx_std_error {
  error_domain_singleton const *domain;
  __SIZE_TYPE__ code;
};

class error {
public:
  error() = delete;
  error(error const &) = delete;
  error(error &&) = delete;
  error &operator=(error const &) = delete;
  error &operator=(error &&) = delete;
  [[nodiscard]] constexpr cxx_std_error release() noexcept;
  constexpr ~error() noexcept {
    auto docleanup{domain_opaque->do_cleanup};
    if (docleanup) docleanup(code_opaque);
  }
  constexpr error_domain_singleton const *domain() const noexcept { return domain_opaque; }
  constexpr __SIZE_TYPE__ code() const noexcept { return code_opaque; }
private:
  error_domain_singleton const *domain_opaque{};
  __SIZE_TYPE__ code_opaque{};
  explicit constexpr error(void const *domain, __SIZE_TYPE__ code) noexcept
      : domain_opaque(static_cast<error_domain_singleton const *>(domain)), code_opaque(code) {}
  friend constexpr error __builtin_herbception_error(void const *, unsigned long);
};

template <typename T>
struct error_domain;

enum class win32_errc : unsigned { success = 0, invalid_function = 1, file_not_found = 2 };

namespace {
constinit error_domain_singleton dummy_domain{};
}

template <>
struct error_domain<win32_errc> {
  using errc_type = win32_errc;
  static constexpr error_domain_singleton const *domain() noexcept { return &dummy_domain; }
  static constexpr __SIZE_TYPE__ code(errc_type e) noexcept {
    return static_cast<unsigned long>(e);
  }
};

// Out-of-class definition, mirroring the libherbceptions header: it needs
// the error_domain<T> specialization above to be complete.
constexpr cxx_std_error error::release() noexcept {
  cxx_std_error e{domain_opaque, code_opaque};
  domain_opaque = error_domain<win32_errc>::domain();
  code_opaque = 0;
  return e;
}

} // namespace std

constexpr int f(int x) throws {
  if (x == 0) throw throws ::std::win32_errc::file_not_found;
  return 2 * x;
}

constexpr int use_release(int x) {
  try {
    return f(x);
  } catch throws(::std::error e) {
    auto rel = e.release();
    return static_cast<int>(rel.code);
  }
}

static_assert(use_release(3) == 6, "success");
static_assert(use_release(0) == 2, "release() yields the code");
