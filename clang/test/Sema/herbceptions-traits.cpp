// RUN: %clang_cc1 -std=c++26 -fherbceptions -fsyntax-only -verify %s
// expected-no-diagnostics

// Herbception type traits: throwsable, is-invoke-fails, the
// invoke_herbceptions_fails_result value_type/error_type query,
// plus the throws-constructible, throws-copy/move-constructible,
// and throws-invocable traits.

// Provide a minimal error_domain specialization so that
// __is_herbceptions_throwsable can be tested.
namespace std {
struct error_domain_singleton {};
template <typename T> struct error_domain;

namespace {
constinit error_domain_singleton dummy_domain{};
}

enum class win32_errc : unsigned { success = 0, file_not_found = 2 };
template <> struct error_domain<win32_errc> {
  using errc_type = win32_errc;
  static constexpr error_domain_singleton const *domain() noexcept {
    return &dummy_domain;
  }
  static constexpr __SIZE_TYPE__ code(errc_type e) noexcept {
    return static_cast<unsigned long>(e);
  }
};

enum class myerr : unsigned { a = 1 };
template <> struct error_domain<myerr> {
  using errc_type = myerr;
  static constexpr error_domain_singleton const *domain() noexcept {
    return &dummy_domain;
  }
  static constexpr __SIZE_TYPE__ code(errc_type e) noexcept {
    return static_cast<unsigned long>(e);
  }
};
} // namespace std

struct Plain {};

int thr(int) throws;
int fls(int) return_failure{std::myerr};
int plain(int);

// __is_herbceptions_throwsable: a usable error_domain<T> means T can be thrown.
static_assert(__is_herbceptions_throwsable(std::win32_errc), "win32_errc");
static_assert(__is_herbceptions_throwsable(std::myerr), "myerr");
static_assert(!__is_herbceptions_throwsable(Plain), "Plain");
static_assert(!__is_herbceptions_throwsable(int), "int");

// __is_invoke_herbceptions_return_failure: only `return_failure{E}` function types, not throws.
static_assert(__is_invoke_herbceptions_return_failure(decltype(fls)), "fls is return_failure");
static_assert(!__is_invoke_herbceptions_return_failure(decltype(thr)), "thr is throws");
static_assert(!__is_invoke_herbceptions_return_failure(decltype(plain)), "plain");
static_assert(!__is_invoke_herbceptions_return_failure(int), "int is not a function");
using Fp = int (*)(int) return_failure{std::myerr};
static_assert(__is_invoke_herbceptions_return_failure(Fp), "fnptr to return_failure");

// __invoke_herbceptions_return_failure_result: {value_type, error_type}.
using R1 = __invoke_herbceptions_return_failure_result<decltype(fls)>;
static_assert(__is_same(typename R1::value_type, int), "fls returns int");
static_assert(__is_same(typename R1::error_type, std::myerr), "fls return_failure{myerr}");

using R2 = __invoke_herbceptions_return_failure_result<decltype(thr)>;
static_assert(__is_same(typename R2::value_type, int), "thr returns int");
static_assert(__is_same(typename R2::error_type, void), "thr has no return_failure type");

using R3 = __invoke_herbceptions_return_failure_result<int>;
static_assert(__is_same(typename R3::value_type, void), "int not a function");
static_assert(__is_same(typename R3::error_type, void), "int has no return_failure type");

using R4 = __invoke_herbceptions_return_failure_result<Fp>;
static_assert(__is_same(typename R4::value_type, int), "fp returns int");
static_assert(__is_same(typename R4::error_type, std::myerr), "fp return_failure{myerr}");

// __is_herbceptions_throws_constructible
struct throws_copy {
  throws_copy(throws_copy const &) throws;
};
struct noexcept_copy {
  noexcept_copy(noexcept_copy const &) noexcept;
};
struct throws_move {
  throws_move(throws_move &&) throws;
};
struct noexcept_move {
  noexcept_move(noexcept_move &&) noexcept;
};

static_assert(__is_herbceptions_throws_constructible(throws_copy, throws_copy const &),
              "throws_copy is throws constructible from const&");
static_assert(!__is_herbceptions_throws_constructible(noexcept_copy, noexcept_copy const &),
              "noexcept_copy is not throws constructible");
static_assert(__is_herbceptions_throws_constructible(throws_move, throws_move &&),
              "throws_move is throws constructible from &&");
static_assert(!__is_herbceptions_throws_constructible(noexcept_move, noexcept_move &&),
              "noexcept_move is not throws constructible");

// __is_herbceptions_throws_copy_constructible (via __is_herbceptions_throws_constructible)
static_assert(__is_herbceptions_throws_constructible(throws_copy, const throws_copy &),
              "throws_copy is throws copy constructible");
static_assert(!__is_herbceptions_throws_constructible(noexcept_copy, const noexcept_copy &),
              "noexcept_copy is not throws copy constructible");
static_assert(__is_herbceptions_throws_constructible(throws_move, throws_move &&),
              "throws_move is throws move constructible");
static_assert(!__is_herbceptions_throws_constructible(noexcept_move, noexcept_move &&),
              "noexcept_move is not throws move constructible");

// __is_herbceptions_throws_invocable
auto throws_func = []() throws {};
auto noexcept_func = []() noexcept {};
auto throws_int_func = []() throws -> int { return 0; };

static_assert(__is_herbceptions_throws_invocable(decltype(throws_func)),
              "throws lambda is throws invocable");
static_assert(!__is_herbceptions_throws_invocable(decltype(noexcept_func)),
              "noexcept lambda is not throws invocable");
static_assert(!__is_herbceptions_throws_invocable(int),
              "int is not invocable");

// __is_herbceptions_throws_invocable_r
static_assert(__is_herbceptions_throws_invocable_r(int, decltype(throws_int_func)),
              "throws_int_func is throws invocable_r int");
static_assert(__is_herbceptions_throws_invocable_r(void, decltype(throws_int_func)),
              "throws_int_func is throws invocable_r void");
static_assert(!__is_herbceptions_throws_invocable_r(int, decltype(throws_func)),
              "throws_func is not throws invocable_r int (void -> int)");
static_assert(!__is_herbceptions_throws_invocable_r(int, int),
              "int is not invocable_r int");

// Function pointers
using ThrowsFp = void (*)() throws;
using NoexceptFp = void (*)() noexcept;
using ThrowsIntFp = int (*)() throws;

static_assert(__is_herbceptions_throws_invocable(ThrowsFp),
              "throws fnptr is throws invocable");
static_assert(!__is_herbceptions_throws_invocable(NoexceptFp),
              "noexcept fnptr is not throws invocable");
static_assert(__is_herbceptions_throws_invocable_r(int, ThrowsIntFp),
              "throws int fnptr is throws invocable_r int");
static_assert(__is_herbceptions_throws_invocable_r(void, ThrowsIntFp),
              "throws int fnptr is throws invocable_r void");
static_assert(!__is_herbceptions_throws_invocable_r(int, ThrowsFp),
              "throws void fnptr is not throws invocable_r int");
