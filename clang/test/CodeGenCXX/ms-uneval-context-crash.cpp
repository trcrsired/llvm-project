// RUN: %clang_cc1 -std=c++20 -fms-compatibility -fms-compatibility-version=19.33 -emit-llvm %s -o - -triple=x86_64-windows-msvc | FileCheck %s

template <typename T>
concept C = requires
{
    { T::test([](){}) };
};

template<typename T>
struct Widget {};

template <C T>
struct Widget<T> {};

struct Baz
{
    template<typename F>
    static constexpr decltype(auto) test(F&&) {}
};

void test()
{
    Widget<Baz> w;
}
// CHECK: @"?test@@YAXXZ"

// A variable template specialization produced while checking a constrained
// variadic function template's requires-clause embeds a lambda whose
// declaration context is the function template itself. Mangling the lambda's
// name requires the function pattern's name, which still contains a pack
// expansion type in its signature.
namespace test2 {
template <typename Cb, typename... A>
inline constexpr bool invocable_ok_v = requires(Cb cb, A... a) { cb(a...); };

template <typename S, typename O, typename... Args>
inline auto foo(S&& s, O&& o, Args&&... args) noexcept
	requires(invocable_ok_v<decltype([](int, unsigned long) noexcept {}), int,
	                         unsigned long>)
{
	return 0;
}

int run_foo() { return foo(1, 2, 'a'); }
// CHECK: @"?run_foo@test2@@YAHXZ"
}
