#include <cstddef>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#undef min
#undef max
#endif

namespace std::error_domains::__herbceptions_detail {

inline void *__malloc_or_heap_alloc_or_die(::std::size_t __sz) noexcept {
#ifdef _WIN32
  void *__bufferptr = HeapAlloc(GetProcessHeap(), 0, __sz);
#else
  void *__bufferptr = ::std::malloc(__sz);
#endif
  if (__bufferptr == nullptr)
    ::std::abort();
  return __bufferptr;
}
inline void __free_or_heap_dealloc(void *__bufferptr) noexcept {
  if (__bufferptr == nullptr)
    return;
#ifdef _WIN32
  HeapFree(GetProcessHeap(), 0, __bufferptr);
#else
  free(__bufferptr);
#endif
}

class __malloc_or_heapalloc_temp_buffer {
public:
  void *__bufferptr{};
  constexpr __malloc_or_heapalloc_temp_buffer() noexcept = default;
  constexpr __malloc_or_heapalloc_temp_buffer(void *__bp) noexcept
      : __bufferptr(__bp) {}
  __malloc_or_heapalloc_temp_buffer(__malloc_or_heapalloc_temp_buffer const &) =
      delete;
  __malloc_or_heapalloc_temp_buffer &
  operator=(__malloc_or_heapalloc_temp_buffer const &) = delete;
  ~__malloc_or_heapalloc_temp_buffer() {
    if (this->__bufferptr == nullptr)
      return;
    ::std::error_domains::__herbceptions_detail::__free_or_heap_dealloc(
        this->__bufferptr);
  }
};
} // namespace std::error_domains::__herbceptions_detail