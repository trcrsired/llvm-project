// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx %s

// Templates and constexpr functions are implicitly inline, so requiring an
// explicit static/inline/extern mark (or re-marking each specialization) is
// noise. An ordinary namespace-scope function still needs the mark.

template <typename... Args>
void tpl(in Args... args) noexcept // no linkage mark needed
{
	(void)sizeof...(args);
}

constexpr int ce(int x) noexcept { return x + 1; }

inline int inl() noexcept { return 2; }

static int st() noexcept { return 3; }

int main()
{
	tpl();
	tpl(1, 2);
	return ce(1) + inl() + st();
}
