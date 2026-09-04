// RUN: %clang_cc1 -std=c++26 -fherbceptions -fcxx-exceptions -fsyntax-only %s
//
// Concepts with throws(false) and throws(true):
// - `throws(false)` is fully equivalent to noexcept(true): cannot fail.
// - `throws(true)` / bare `throws` uses the herbception model: the function
//   may fail via the error channel. In trait queries (noexcept(expr),
//   is_nothrow) it behaves as noexcept(false).
// - In requires-expr, `noexcept` matches only `throws(false)`.
// - `{foo(t)} throws` is false for `throws(false)` (cannot fail), true otherwise.
// - `throws` and `noexcept` are mutually exclusive; throws supersedes noexcept.

struct bar {};

constexpr void foo_throws_false(bar) throws(false) {}
constexpr void foo_throws_true(bar) throws(true) {}
constexpr void foo_throws(bar) throws {}
constexpr void foo_noexcept(bar) noexcept {}

template<typename T>
concept is_throws_false = requires(T t)
{
    {foo_throws_false(t)} throws;
};

template<typename T>
concept is_throws_true = requires(T t)
{
    {foo_throws_true(t)} throws;
};

template<typename T>
concept is_throws = requires(T t)
{
    {foo_throws(t)} throws;
};

template<typename T>
concept is_noexcept_false = requires(T t)
{
    {foo_throws_false(t)} noexcept;
};

template<typename T>
concept is_noexcept_true = requires(T t)
{
    {foo_throws_true(t)} noexcept;
};

template<typename T>
concept is_noexcept_plain = requires(T t)
{
    {foo_noexcept(t)} noexcept;
};

// throws(false) cannot fail: {foo(t)} throws should be false
static_assert(!is_throws_false<bar>);

// throws(true) can fail: {foo(t)} throws should be true
static_assert(is_throws_true<bar>);

// bare throws can fail: {foo(t)} throws should be true
static_assert(is_throws<bar>);

// throws(false) is noexcept: {foo(t)} noexcept should be true
static_assert(is_noexcept_false<bar>);

// throws(true) is NOT noexcept: {foo(t)} noexcept should be false
static_assert(!is_noexcept_true<bar>);

// plain noexcept: {foo(t)} noexcept should be true
static_assert(is_noexcept_plain<bar>);

// noexcept(expr) for throws(true) should be false
static_assert(!noexcept(foo_throws_true(bar())), "throws(true) is not noexcept");

// noexcept(expr) for throws(false) should be true
static_assert(noexcept(foo_throws_false(bar())), "throws(false) is noexcept");

// noexcept(expr) for bare throws should be false
static_assert(!noexcept(foo_throws(bar())), "bare throws is not noexcept");

// The equivalence is a type-system and ABI property, not merely the result of
// a noexcept query. A disabled channel has the ordinary noexcept function type
// and must not satisfy invocation traits for the herbception channel.
using throws_false_type = void(bar) throws(false);
using noexcept_type = void(bar) noexcept;
using throws_true_type = void(bar) throws(true);
using throws_type = void(bar) throws;
static_assert(__is_same(throws_false_type, noexcept_type));
static_assert(!__is_same(throws_false_type, throws_true_type));
static_assert(!__is_same(throws_false_type, throws_type));
static_assert(__is_same(throws_true_type, throws_type));
static_assert(!throws(foo_throws_false(bar())));
static_assert(!__is_herbceptions_throws_invocable(throws_false_type, bar));
static_assert(__is_herbceptions_throws_invocable(throws_true_type, bar));

// Equivalent non-throwing spellings may redeclare one function in either
// order; they cannot create an ABI disagreement between declarations.
void redeclared_noexcept_first(bar) noexcept;
void redeclared_noexcept_first(bar) throws(false);
void redeclared_throws_false_first(bar) throws(false);
void redeclared_throws_false_first(bar) noexcept;
void redeclared_throws_first(bar) throws;
void redeclared_throws_first(bar) throws(true);
void redeclared_throws_true_first(bar) throws(true);
void redeclared_throws_true_first(bar) throws;

noexcept_type *from_throws_false = &foo_throws_false;
throws_false_type *from_noexcept = &foo_noexcept;


// throws and noexcept cannot be combined
// void bad1() throws(false) noexcept; // error
// void bad2() noexcept throws(true); // error
// void bad3() throws(true) noexcept(false); // error
