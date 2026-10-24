//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once
/*
Self-contained WinAPI import surface for the runtime (no <windows.h>).
*/
#include <cstddef>
#include <cstdint>

#if defined(__MINGW32__) && __has_include(<_mingw.h>)
// Defines _UCRT and derives _WIN32_WINDOWS; no C++ header guarantees it.
#include <_mingw.h>
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#pragma comment(lib, "ntdll.lib")
#endif

namespace std::error_domains::__herbceptions_detail {

/*
A entry points on 9x, W on NT: Windows 95 has no W functions, and a W import
kernel32 does not export fails at load time, so legacy-msvcrt toolchains and
explicit 9x targets take the A path. UCRT and MSVC builds keep the W ones.
*/
inline constexpr bool __win32_use_9xa_apis{
#if defined(_WIN32_WINDOWS) || (!defined(_MSC_VER) && !defined(_UCRT))
    true
#endif
};

namespace win32 {

/*
These are declared with C++ linkage and bound to their import symbols
through an asm label instead of extern "C": a translation unit that already
included <windows.h> has C declarations of the same names (DWORD, WINAPI,
va_list *), which would otherwise clash with these redeclarations. The stack
byte count only feeds the __stdcall decoration on i386; MSVC proper has no
asm labels and needs extern "C" instead. On i386 GCC adds the user label
underscore to the asm label itself while clang takes it verbatim, so only
the clang label spells it out. ARM64EC import thunks carry the '#' EC
decoration.
*/
#if defined(_MSC_VER) && !defined(__clang__)
extern "C" {
#define __HERBCEPTIONS_WIN32_IMPORT(name, stackbytes)
#elif defined(_M_HYBRID)
#define __HERBCEPTIONS_WIN32_IMPORT(name, stackbytes)                          \
  __asm__("#" #name "@" #stackbytes)
#elif defined(__arm64ec__)
#define __HERBCEPTIONS_WIN32_IMPORT(name, stackbytes) __asm__("#" #name)
#elif defined(__i386__) && defined(__clang__)
#define __HERBCEPTIONS_WIN32_IMPORT(name, stackbytes)                          \
  __asm__("_" #name "@" #stackbytes)
#elif defined(__i386__)
#define __HERBCEPTIONS_WIN32_IMPORT(name, stackbytes)                          \
  __asm__(#name "@" #stackbytes)
#else
#define __HERBCEPTIONS_WIN32_IMPORT(name, stackbytes) __asm__(#name)
#endif

/*
__stdcall only picks the callee-pops convention on i386; every other Windows
target has a single calling convention, where GCC warns that the attribute is
ignored. Left defined past this header for the DllMain entry point in
msvc_exception_ptr.cpp.
*/
#if defined(__i386__) || defined(_M_IX86)
#define __HERBCEPTIONS_WIN32_STDCALL __stdcall
#else
#define __HERBCEPTIONS_WIN32_STDCALL
#endif

// ntdll
__declspec(dllimport) void *__HERBCEPTIONS_WIN32_STDCALL RtlAllocateHeap(
    void *heap, ::std::uint_least32_t flags, ::std::size_t size) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(RtlAllocateHeap, 12);
__declspec(dllimport) int __HERBCEPTIONS_WIN32_STDCALL
RtlFreeHeap(void *heap, ::std::uint_least32_t flags, void *ptr) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(RtlFreeHeap, 12);
__declspec(dllimport) void *__HERBCEPTIONS_WIN32_STDCALL
RtlPcToFileHeader(void const *pc, void **base) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(RtlPcToFileHeader, 8);

// kernel32
__declspec(dllimport) void *__HERBCEPTIONS_WIN32_STDCALL
GetProcessHeap() noexcept __HERBCEPTIONS_WIN32_IMPORT(GetProcessHeap, 0);
__declspec(dllimport) void *__HERBCEPTIONS_WIN32_STDCALL
HeapAlloc(void *heap, ::std::uint_least32_t flags, ::std::size_t bytes) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(HeapAlloc, 12);
__declspec(dllimport) int __HERBCEPTIONS_WIN32_STDCALL
HeapFree(void *heap, ::std::uint_least32_t flags, void *ptr) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(HeapFree, 12);
__declspec(dllimport) void *__HERBCEPTIONS_WIN32_STDCALL
LocalFree(void *mem) noexcept __HERBCEPTIONS_WIN32_IMPORT(LocalFree, 4);
__declspec(dllimport) void __HERBCEPTIONS_WIN32_STDCALL RaiseException(
    ::std::uint_least32_t exception_code, ::std::uint_least32_t exception_flags,
    ::std::uint_least32_t number_parameters,
    ::std::uintptr_t const *exception_information) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(RaiseException, 16);
__declspec(dllimport) ::std::uint_least32_t __HERBCEPTIONS_WIN32_STDCALL
FormatMessageA(::std::uint_least32_t flags, void const *source,
               ::std::uint_least32_t message_id,
               ::std::uint_least32_t language_id, char *buffer,
               ::std::uint_least32_t size, char *arguments) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(FormatMessageA, 28);
__declspec(dllimport) ::std::uint_least32_t __HERBCEPTIONS_WIN32_STDCALL
FormatMessageW(::std::uint_least32_t flags, void const *source,
               ::std::uint_least32_t message_id,
               ::std::uint_least32_t language_id, wchar_t *buffer,
               ::std::uint_least32_t size, char *arguments) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(FormatMessageW, 28);
__declspec(dllimport) void *__HERBCEPTIONS_WIN32_STDCALL GetModuleHandleA(
    char const *name) noexcept __HERBCEPTIONS_WIN32_IMPORT(GetModuleHandleA, 4);
__declspec(dllimport) void *__HERBCEPTIONS_WIN32_STDCALL
GetModuleHandleW(wchar_t const *name) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(GetModuleHandleW, 4);
__declspec(dllimport) void *__HERBCEPTIONS_WIN32_STDCALL
GetProcAddress(void *module, char const *name) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(GetProcAddress, 8);

#if defined(_MSC_VER) && !defined(__clang__)
}
#endif

#undef __HERBCEPTIONS_WIN32_IMPORT

} // namespace win32

// Minimal NT type the runtime touches: EXCEPTION_RECORD.
struct exception_record {
  ::std::uint_least32_t ExceptionCode;
  ::std::uint_least32_t ExceptionFlags;
  void *ExceptionRecord;
  void *ExceptionAddress;
  ::std::uint_least32_t NumberParameters;
  ::std::uintptr_t ExceptionInformation[15];
};

using EXCEPTION_RECORD = exception_record;

} // namespace std::error_domains::__herbceptions_detail
