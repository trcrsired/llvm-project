// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx %s

// A function template instantiated from inside a main-file [[unsafe]] fence
// must not inherit an implicit [[unsafe]] call restriction. The restriction is
// for functions *declared* by a fenced #include (legacy import); a
// specialization is declared in the header of the pattern, and inherits any
// attribute from that pattern through isUnsafeToCall() already.
//
// Before the fix, a span(from_range, vector) construction inside a fence
// instantiated vector's members and made later push_back/operator[] calls
// outside the fence fail with "call to ... is unsafe".

template <typename T>
struct box {
	void put(T) noexcept {}
	T get() noexcept { return T{}; }
};

template <typename T>
void use_box(T v) noexcept
{
	box<T> b;
	b.put(v);
	(void)b.get();
}

[[unsafe]] {
// Instantiates box<int>::put and box<int>::get while the fence is active.
void trigger() noexcept { use_box(1); }
}

int main()
{
	box<int> b;   // must stay callable from safe source
	b.put(1);
	return b.get();
}
