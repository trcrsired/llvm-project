// A library header that fences its own pointer-using kernel. Including it
// normally must not make the library's interface unsafe to call.
#pragma once

[[unsafe]] {
struct Widget {
  int *storage = nullptr;
  unsigned long n = 0;
  int size() const noexcept { return static_cast<int>(n); }
};
}
