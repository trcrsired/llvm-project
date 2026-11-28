// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx %s

// Regression: the declaration-level view classification inspected bases() and
// fields() of an incomplete record declaration. An opaque pointer to a
// forward-declared struct used inside a fenced region used to crash the
// lifetime analysis when it built the pointee's origin list.

[[unsafe]] {
struct incomplete; // never defined

inline void use(incomplete *p) noexcept { (void)p; }
} // end of fence

static int f() noexcept {
	incomplete *p = nullptr;
	(void)p;
	return 0;
}

int main() { return f(); }
