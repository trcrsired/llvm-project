#pragma once
/*
Self-contained WinAPI import surface for the runtime (no <windows.h>).

Every function is declared __declspec(dllimport) + noexcept. Symbol binding
mirrors fast_io_core_impl/allocation: real MSVC keeps its own __stdcall
decoration and binds through /alternatename pragmas (mangled names below are
for namespace std::error_domains::__herbceptions_detail::win32); every other
frontend binds through __asm__ symbol renames (undecorated on x64/ARM64,
underscore/@byte stdcall decoration on x86).
*/
#include <cstddef>
#include <cstdint>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#pragma comment(lib, "ntdll.lib")
#endif

#pragma push_macro("HB_STDCALL")
#pragma push_macro("HB_RENAME")
#undef HB_STDCALL
#undef HB_RENAME
#if defined(_MSC_VER) && !defined(__clang__)
#define HB_STDCALL __stdcall
#define HB_RENAME(name, count)
#elif defined(__clang__) || defined(__GNUC__)
#define HB_STDCALL __attribute__((__stdcall__))
#if defined(_M_HYBRID)
#define HB_RENAME(name, count) __asm__("#" #name "@" #count)
#elif defined(__arm64ec__)
#define HB_RENAME(name, count) __asm__("#" #name)
#elif SIZE_MAX <= UINT_LEAST32_MAX &&                                     \
    (defined(__x86__) || defined(_M_IX86) || defined(__i386__))
#if !defined(__clang__)
#define HB_RENAME(name, count) __asm__(#name "@" #count)
#else
#define HB_RENAME(name, count) __asm__("_" #name "@" #count)
#endif
#else
#define HB_RENAME(name, count) __asm__(#name)
#endif
#else
#define HB_STDCALL __stdcall
#define HB_RENAME(name, count)
#endif

namespace std::error_domains::__herbceptions_detail {

namespace win32 {

inline constexpr ::std::uint_least32_t format_message_from_system{
    0x00001000u};
inline constexpr ::std::uint_least32_t format_message_ignore_inserts{
    0x00000200u};
inline constexpr ::std::uint_least32_t format_message_allocate_buffer{
    0x00000100u};
inline constexpr ::std::uint_least32_t format_message_from_hmodule{
    0x00000800u};

inline constexpr ::std::uint_least16_t lang_english{0x09u};
inline constexpr ::std::uint_least16_t sublang_english_us{0x01u};

} // namespace win32

// MAKELANGID(primary, sub): primary in bits 10-15, sub in 0-9.
#define HB_MAKE_LANGID(primary, sub)                                           \
  (static_cast<::std::uint_least16_t>(                                         \
      (static_cast<::std::uint_least16_t>(primary) << 10u) |                   \
      static_cast<::std::uint_least16_t>(sub)))

namespace win32 {

extern "C" {
// ntdll
__declspec(dllimport) void *HB_STDCALL RtlAllocateHeap(
    void *heap, unsigned long flags,
    ::std::size_t size) noexcept HB_RENAME(RtlAllocateHeap, 12);
__declspec(dllimport) unsigned char HB_STDCALL RtlFreeHeap(
    void *heap, unsigned long flags,
    void *ptr) noexcept HB_RENAME(RtlFreeHeap, 12);
__declspec(dllimport) void *HB_STDCALL RtlPcToFileHeader(
    void const *pc, void **base) noexcept HB_RENAME(RtlPcToFileHeader, 8);

// kernel32
__declspec(dllimport) void *HB_STDCALL GetProcessHeap()
    noexcept HB_RENAME(GetProcessHeap, 0);
__declspec(dllimport) void *HB_STDCALL HeapAlloc(
    void *heap, unsigned long flags,
    ::std::size_t bytes) noexcept HB_RENAME(HeapAlloc, 12);
__declspec(dllimport) unsigned char HB_STDCALL HeapFree(
    void *heap, unsigned long flags,
    void *ptr) noexcept HB_RENAME(HeapFree, 12);
__declspec(dllimport) void *HB_STDCALL LocalFree(
    void *mem) noexcept HB_RENAME(LocalFree, 4);
__declspec(dllimport) void *HB_STDCALL GetModuleHandleA(
    char const *name) noexcept HB_RENAME(GetModuleHandleA, 4);
__declspec(dllimport) void *HB_STDCALL GetModuleHandleW(
    wchar_t const *name) noexcept HB_RENAME(GetModuleHandleW, 4);
__declspec(dllimport) unsigned long HB_STDCALL FormatMessageA(
    unsigned long flags, void const *source, unsigned long message_id,
    unsigned long language_id, char *buffer, unsigned long size,
    char *arguments) noexcept HB_RENAME(FormatMessageA, 28);
__declspec(dllimport) unsigned long HB_STDCALL FormatMessageW(
    unsigned long flags, void const *source, unsigned long message_id,
    unsigned long language_id, wchar_t *buffer, unsigned long size,
    char *arguments) noexcept HB_RENAME(FormatMessageW, 28);
}

} // namespace win32

#pragma pop_macro("HB_RENAME")
#pragma pop_macro("HB_STDCALL")

// Minimal NT type the runtime touches: EXCEPTION_RECORD.
struct exception_record {
  unsigned long ExceptionCode;
  unsigned long ExceptionFlags;
  void *ExceptionRecord;
  void *ExceptionAddress;
  unsigned long NumberParameters;
  ::std::uintptr_t ExceptionInformation[15];
};

using EXCEPTION_RECORD = exception_record;

#if defined(_MSC_VER) && !defined(__clang__)
// Bind the C++-mangled dllimport thunks above to the real undecorated DLL
// imports (x64 first, then x86). Manglings are for the namespace
// std::error_domains::__herbceptions_detail::win32.
#if SIZE_MAX > UINT_LEAST32_MAX
#pragma comment(linker, "/alternatename:__imp_?RtlAllocateHeap@win32@__herbceptions_detail@error_domains@std@@YAPEAXPEAXI_K@Z=__imp_RtlAllocateHeap")
#pragma comment(linker, "/alternatename:__imp_?RtlFreeHeap@win32@__herbceptions_detail@error_domains@std@@YAEPEAXI0@Z=__imp_RtlFreeHeap")
#pragma comment(linker, "/alternatename:__imp_?RtlPcToFileHeader@win32@__herbceptions_detail@error_domains@std@@YAPEAXPEBXPEAPEAX@Z=__imp_RtlPcToFileHeader")
#pragma comment(linker, "/alternatename:__imp_?GetProcessHeap@win32@__herbceptions_detail@error_domains@std@@YAPEAXXZ=__imp_GetProcessHeap")
#pragma comment(linker, "/alternatename:__imp_?HeapAlloc@win32@__herbceptions_detail@error_domains@std@@YAPEAXPEAXI_K@Z=__imp_HeapAlloc")
#pragma comment(linker, "/alternatename:__imp_?HeapFree@win32@__herbceptions_detail@error_domains@std@@YAEPEAXI0@Z=__imp_HeapFree")
#pragma comment(linker, "/alternatename:__imp_?LocalFree@win32@__herbceptions_detail@error_domains@std@@YAPEAXPEAX@Z=__imp_LocalFree")
#pragma comment(linker, "/alternatename:__imp_?GetModuleHandleA@win32@__herbceptions_detail@error_domains@std@@YAPEAXPEBD@Z=__imp_GetModuleHandleA")
#pragma comment(linker, "/alternatename:__imp_?GetModuleHandleW@win32@__herbceptions_detail@error_domains@std@@YAPEAXPEA_W@Z=__imp_GetModuleHandleW")
#pragma comment(linker, "/alternatename:__imp_?FormatMessageA@win32@__herbceptions_detail@error_domains@std@@YAIPEBDIPEAXIPEAD@Z=__imp_FormatMessageA")
#pragma comment(linker, "/alternatename:__imp_?FormatMessageW@win32@__herbceptions_detail@error_domains@std@@YAIPEBXIPEA_WIPEAD@Z=__imp_FormatMessageW")
#else
#pragma comment(linker, "/alternatename:__imp_?RtlAllocateHeap@win32@__herbceptions_detail@error_domains@std@@YGPAXPAXI_K@Z=__imp__RtlAllocateHeap@12")
#pragma comment(linker, "/alternatename:__imp_?RtlFreeHeap@win32@__herbceptions_detail@error_domains@std@@YGEPAXI0@Z=__imp__RtlFreeHeap@12")
#pragma comment(linker, "/alternatename:__imp_?RtlPcToFileHeader@win32@__herbceptions_detail@error_domains@std@@YGPAXPBXPAPAX@Z=__imp__RtlPcToFileHeader@8")
#pragma comment(linker, "/alternatename:__imp_?GetProcessHeap@win32@__herbceptions_detail@error_domains@std@@YGPAXXZ=__imp__GetProcessHeap@0")
#pragma comment(linker, "/alternatename:__imp_?HeapAlloc@win32@__herbceptions_detail@error_domains@std@@YGPAXPAXI@Z=__imp__HeapAlloc@12")
#pragma comment(linker, "/alternatename:__imp_?HeapFree@win32@__herbceptions_detail@error_domains@std@@YGEPAXI0@Z=__imp__HeapFree@12")
#pragma comment(linker, "/alternatename:__imp_?LocalFree@win32@__herbceptions_detail@error_domains@std@@YGPAXPAX@Z=__imp__LocalFree@4")
#pragma comment(linker, "/alternatename:__imp_?GetModuleHandleA@win32@__herbceptions_detail@error_domains@std@@YGPAXPBD@Z=__imp__GetModuleHandleA@4")
#pragma comment(linker, "/alternatename:__imp_?GetModuleHandleW@win32@__herbceptions_detail@error_domains@std@@YGPAXPB_W@Z=__imp__GetModuleHandleW@4")
#pragma comment(linker, "/alternatename:__imp_?FormatMessageA@win32@__herbceptions_detail@error_domains@std@@YGIPBDIPAXIPAD@Z=__imp__FormatMessageA@28")
#pragma comment(linker, "/alternatename:__imp_?FormatMessageW@win32@__herbceptions_detail@error_domains@std@@YGIPBZIPA_WIPAD@Z=__imp__FormatMessageW@28")
#endif
#endif

} // namespace std::error_domains::__herbceptions_detail
