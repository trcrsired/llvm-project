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

/*
Two heap flavors live side by side:

  - The NT flavor goes straight to ntdll's RtlAllocateHeap/RtlFreeHeap with
    the process heap handle read out of the PEB through the TEB ("register"
    part), so no kernel32 import is needed for it. Used by the exception
    domains, which only ever run on NT-family kernels. The declarations
    mirror fast_io_core_impl/allocation/nt_preliminary_definition.h.
  - The win32 flavor uses kernel32's HeapAlloc/HeapFree, which exists since
    Windows 95; used by the win32/com message machinery and by the itanium
    domain so they keep working everywhere.

Every imported function is declared noexcept so no exception semantics can
bite across the C ABI boundary.
*/
namespace std::error_domains::__herbceptions_detail {

#ifdef _WIN32

#if defined(_MSC_VER) && !defined(_KERNEL_MODE) && !defined(_WIN32_WINDOWS)
#pragma comment(lib, "ntdll.lib")
#endif

/*
Import declarations follow fast_io's nt_preliminary_definition.h scheme:
real MSVC keeps its own __stdcall decoration, every other frontend binds
the symbol through an __asm__ rename so 32-bit underscore/@byte decoration
comes out right without import libraries.
*/
#pragma push_macro("__HB_STDCALL")
#pragma push_macro("__HB_NT_RENAME")
#undef __HB_STDCALL
#undef __HB_NT_RENAME
#if defined(_MSC_VER) && !defined(__clang__)
#define __HB_STDCALL __stdcall
#define __HB_NT_RENAME(name, count)
#elif defined(__clang__) || defined(__GNUC__)
#define __HB_STDCALL __attribute__((__stdcall__))
#if defined(_M_HYBRID)
#define __HB_NT_RENAME(name, count) __asm__("#" #name "@" #count)
#elif defined(__arm64ec__)
#define __HB_NT_RENAME(name, count) __asm__("#" #name)
#elif SIZE_MAX <= UINT_LEAST32_MAX &&                                     \
    (defined(__x86__) || defined(_M_IX86) || defined(__i386__))
#if !defined(__clang__)
#define __HB_NT_RENAME(name, count) __asm__(#name "@" #count)
#else
#define __HB_NT_RENAME(name, count) __asm__("_" #name "@" #count)
#endif
#else
#define __HB_NT_RENAME(name, count) __asm__(#name)
#endif
#else
#define __HB_STDCALL __stdcall
#define __HB_NT_RENAME(name, count)
#endif

extern "C" {
__declspec(dllimport) void *__HB_STDCALL RtlAllocateHeap(
    void *heap, unsigned long flags, ::std::size_t size) noexcept __HB_NT_RENAME(
        RtlAllocateHeap, 12);
__declspec(dllimport) unsigned char __HB_STDCALL
RtlFreeHeap(void *heap, unsigned long flags,
            void *ptr) noexcept __HB_NT_RENAME(RtlFreeHeap, 12);
}

#pragma pop_macro("__HB_NT_RENAME")
#pragma pop_macro("__HB_STDCALL")

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

// NT flavor: ntdll heap on the PEB process heap.
inline void *__nt_heap_alloc_or_die(::std::size_t __sz) noexcept {
  void *__bufferptr = RtlAllocateHeap(__process_heap(), 0, __sz);
  if (__bufferptr == nullptr)
    ::std::abort();
  return __bufferptr;
}
inline void __nt_heap_free(void *__bufferptr) noexcept {
  if (__bufferptr == nullptr)
    return;
  RtlFreeHeap(__process_heap(), 0, __bufferptr);
}

// Win32 flavor: kernel32 heap, available on every Windows including 9x.
inline void *__win32_heap_alloc_or_die(::std::size_t __sz) noexcept {
  void *__bufferptr = ::HeapAlloc(::GetProcessHeap(), 0, __sz);
  if (__bufferptr == nullptr)
    ::std::abort();
  return __bufferptr;
}
inline void __win32_heap_free(void *__bufferptr) noexcept {
  if (__bufferptr == nullptr)
    return;
  ::HeapFree(::GetProcessHeap(), 0, __bufferptr);
}

#endif

// Dispatchers used by the runtime code: the nt flavor for the exception
// domains, the win32 flavor elsewhere.
inline void *__malloc_or_heap_alloc_or_die(::std::size_t __sz) noexcept {
#ifdef _WIN32
  return __nt_heap_alloc_or_die(__sz);
#else
  void *__bufferptr = ::std::malloc(__sz);
  if (__bufferptr == nullptr)
    ::std::abort();
  return __bufferptr;
#endif
}
inline void __free_or_heap_dealloc(void *__bufferptr) noexcept {
#ifdef _WIN32
  __nt_heap_free(__bufferptr);
#else
  if (__bufferptr != nullptr) {
    ::std::free(__bufferptr);
  }
#endif
}
inline void *__malloc_or_die(::std::size_t __sz) noexcept {
  void *__bufferptr = ::std::malloc(__sz);
  if (__bufferptr == nullptr)
    ::std::abort();
  return __bufferptr;
}

template <unsigned __malloconly = 0> class __basic_malloc_or_heapalloc_temp_buffer {
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
    }
#ifdef _WIN32
    else if constexpr (__malloconly == 3) {
      __win32_heap_free(this->__bufferptr);
    }
#endif
    else {
      __free_or_heap_dealloc(this->__bufferptr);
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
using __heapalloc_temp_buffer = __basic_malloc_or_heapalloc_temp_buffer<
#ifdef _WIN32
    3
#else
    0
#endif
    >;

} // namespace std::error_domains::__herbceptions_detail
