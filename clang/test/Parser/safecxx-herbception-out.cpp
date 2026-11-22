// RUN: %clang_cc1 -fsyntax-only -fsafecxx -fherbceptions -verify %s
// expected-no-diagnostics

// Regression: a try/catch whose try block can terminate before reaching the
// fall-through produced an unreachable CFG edge with a null target. The
// definite-first-use dataflow walked the predecessor list without skipping it
// and dereferenced the null block.

namespace std {
struct error {
  void *domain;
  unsigned long code;
};
enum class errc : unsigned { io_error = 5 };
template <typename T> class error_domain;
template <> class error_domain<errc> {
public:
  static void *domain() noexcept;
  static unsigned long code(errc) noexcept;
};
} // namespace std

// Constructs before it can fail, so every path is fine. The try body's
// fall-through is an unreachable edge.
static void throws_path(out int v, bool ok) throws {
  v = 0;
  try {
    if (ok)
      throw throws ::std::errc::io_error;
  } catch throws (std::error e) {
    (void)e;
  }
}
