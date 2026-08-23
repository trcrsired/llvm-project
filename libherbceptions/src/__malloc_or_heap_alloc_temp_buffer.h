#include <cstddef>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#undef min
#undef max
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#endif

namespace std::error_domains::__herbceptions_detail {

#ifdef _WIN32

/*
The heap routines are reached directly through ntdll and the process heap
handle is read out of the PEB through the TEB, so no kernel32 import is
needed for them. They are declared noexcept here so no exception semantics
can ever bite across the C ABI boundary.
*/
extern "C" {
__declspec(dllimport) void *__stdcall RtlAllocateHeap(
    void *heap, unsigned long flags, ::std::size_t size) noexcept;
__declspec(dllimport) unsigned char __stdcall RtlFreeHeap(void *heap,
                                                          unsigned long flags,
                                                          void *ptr) noexcept;
}

inline void *__process_heap() noexcept {
#if defined(_M_X64) || defined(__x86_64__)
  // TEB -> PEB (gs:[0x60]); PEB->ProcessHeap lives at 0x30.
  void *const peb{reinterpret_cast<void *>(__readgsqword(0x60))};
  return *reinterpret_cast<void **>(
      reinterpret_cast<unsigned char *>(peb) + 0x30);
#elif defined(_M_IX86) || defined(__i386__)
  // TEB -> PEB (fs:[0x30]); PEB->ProcessHeap lives at 0x18.
  void *const peb{*reinterpret_cast<void **>(
      static_cast<::std::uintptr_t>(__readfsdword(0x30)))};
  return *reinterpret_cast<void **>(
      reinterpret_cast<unsigned char *>(peb) + 0x18);
#else
  return ::GetProcessHeap();
#endif
}

#endif

inline void *__malloc_or_heap_alloc_or_die(::std::size_t __sz) noexcept {
#ifdef _WIN32
  void *__bufferptr =
      RtlAllocateHeap(__process_heap(), 0, __sz);
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
  RtlFreeHeap(__process_heap(), 0, __bufferptr);
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
      ::LocalFree(this->__bufferptr);
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
