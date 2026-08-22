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

inline void *__malloc_or_die(::std::size_t __sz) noexcept {
  void *__bufferptr = ::std::malloc(__sz);
  if (__bufferptr == nullptr)
    ::std::abort();
  return __bufferptr;
}

template <unsigned __malloconly = 0>
class __basic_malloc_or_heapalloc_temp_buffer {
public:
  void *__bufferptr{};
  constexpr __basic_malloc_or_heapalloc_temp_buffer() noexcept = default;
  constexpr __basic_malloc_or_heapalloc_temp_buffer(void *__bp) noexcept
      : __bufferptr(__bp) {}
  __basic_malloc_or_heapalloc_temp_buffer(
      __basic_malloc_or_heapalloc_temp_buffer const &) = delete;
  __basic_malloc_or_heapalloc_temp_buffer &
  operator=(__basic_malloc_or_heapalloc_temp_buffer const &) = delete;
  ~__basic_malloc_or_heapalloc_temp_buffer() {
    if (this->__bufferptr == nullptr)
      return;
#ifdef _WIN32
    if constexpr (__malloconly == 2) {
      LocalFree(this->__bufferptr);
    } else
#endif
        if constexpr (__malloconly == 1) {
      ::std::free(this->__bufferptr);
    } else {
      ::std::error_domains::__herbceptions_detail::__free_or_heap_dealloc(
          this->__bufferptr);
    }
  }
};

using __malloc_or_heapalloc_temp_buffer =
    __basic_malloc_or_heapalloc_temp_buffer<0>;
using __local_free_temp_buffer = __basic_malloc_or_heapalloc_temp_buffer<
#ifdef _WIN32
    2
#else
    0
#endif
    >;
using __malloc_temp_buffer = __basic_malloc_or_heapalloc_temp_buffer<
#ifdef _WIN32
    1
#else
    0
#endif
    >;
} // namespace std::error_domains::__herbceptions_detail
