// RUN: %clang_cc1 -std=c++26 -fherbceptions -fcxx-exceptions -fsyntax-only %s
//
// Concepts with throws(false) and throws(true):
// - `throws`, `throws(true)`, `throws(false)` all imply noexcept(true).
// - In requires-expr, `noexcept` matches only throws(false) (the only one
//   that is actually noexcept). `throws(true)` and bare `throws` have
//   CT_Deterministic and are NOT noexcept.
// - `{foo(t)} throws` is false for throws(false) (cannot fail), true otherwise.

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

// throws(false) implies noexcept(true): {foo(t)} noexcept should be true
static_assert(is_noexcept_false<bar>);

// throws(true) is not noexcept: {foo(t)} noexcept should be false
static_assert(!is_noexcept_true<bar>);

// plain noexcept: {foo(t)} noexcept should be true
static_assert(is_noexcept_plain<bar>);
