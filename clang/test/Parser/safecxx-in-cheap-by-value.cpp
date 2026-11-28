// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx %s

// An `in T` parameter crosses by value when T is cheap to copy, by const
// reference otherwise. For a dependent T the decision is deferred until
// instantiation, when T becomes known.

template <typename T>
static void f(in T t) noexcept
{
	(void)t;
}

static_assert(__is_same(decltype(f<int>), void(const int) noexcept));

struct big {
	long long a, b, c, d, e, f, g, h;
};

[[unsafe]] {
static_assert(!__is_same(decltype(f<int>), void(const int &) noexcept));
static_assert(__is_same(decltype(f<big>), void(const big &) noexcept));
}

int main() {}
