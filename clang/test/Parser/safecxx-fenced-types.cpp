// RUN: %clang_cc1 -std=c++26 -fsyntax-only -fsafecxx -verify %s

// A typedef or enum class is a type, not an operation. Declaring it inside an
// [[unsafe]] fence does not make it unsafe to name: only functions get the
// implicit fence call restriction, and an explicit [[unsafe]] mark is what
// restricts a declaration. This keeps aliases such as std::size_t and
// std::uint_least32_t -- and std::byte, an enum class -- usable from safe
// source even when they arrive through a fenced implementation header.

[[unsafe]] {
using size_type = unsigned long;       // like std::size_t
typedef unsigned int uint_least_type;  // like std::uint_least32_t
enum class byte_like : unsigned char {}; // like std::byte
enum legacy_enum { zero, one };
struct legacy {
  using nested_size = unsigned long;
  enum class nested_E { a };
};
}

static size_type f(uint_least_type x) noexcept { return x; }
static byte_like g(legacy_enum e) noexcept { return static_cast<byte_like>(e); }
static void h(legacy::nested_size, legacy::nested_E) noexcept {}

static void safe_use() noexcept {
  size_type s = f(1);
  (void)g(zero);
  h(s, legacy::nested_E::a);
}

// The ban is on the type named, not on the spelling "typedef": a typedef to a
// reference still names a reference and is rejected in safe source.
[[unsafe]] {
using ref_type = int &;
}
static void bad(ref_type) noexcept {} // expected-error {{raw pointer or reference type}}
