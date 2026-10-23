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

#if defined(_MSC_VER) && !defined(__clang__)
#pragma comment(lib, "ntdll.lib")
#endif

namespace std::error_domains::__herbceptions_detail {

namespace win32 {

/*
These are declared with C++ linkage and bound to their import symbols
through an asm label instead of extern "C": a translation unit that already
included <windows.h> has C declarations of the same names (DWORD, WINAPI,
va_list *), which would otherwise clash with these redeclarations. The stack
byte count only feeds the __stdcall decoration on i386; MSVC proper has no
asm labels and needs extern "C" instead. On i386 GCC adds the user label
underscore to the asm label itself while clang takes it verbatim, so only
the clang label spells it out.
*/
#if defined(_MSC_VER) && !defined(__clang__)
extern "C" {
#define __HERBCEPTIONS_WIN32_IMPORT(name, stackbytes)
#elif defined(__i386__) && defined(__clang__)
#define __HERBCEPTIONS_WIN32_IMPORT(name, stackbytes)                          \
  __asm__("_" #name "@" #stackbytes)
#elif defined(__i386__)
#define __HERBCEPTIONS_WIN32_IMPORT(name, stackbytes)                          \
  __asm__(#name "@" #stackbytes)
#else
#define __HERBCEPTIONS_WIN32_IMPORT(name, stackbytes) __asm__(#name)
#endif

// ntdll
__declspec(dllimport) void *__stdcall
RtlAllocateHeap(void *heap, ::std::uint_least32_t flags,
                ::std::size_t size) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(RtlAllocateHeap, 12);
__declspec(dllimport) int __stdcall
RtlFreeHeap(void *heap, ::std::uint_least32_t flags, void *ptr) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(RtlFreeHeap, 12);
__declspec(dllimport) void *__stdcall RtlPcToFileHeader(void const *pc,
                                                        void **base) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(RtlPcToFileHeader, 8);

// kernel32
__declspec(dllimport) void *__stdcall GetProcessHeap() noexcept
    __HERBCEPTIONS_WIN32_IMPORT(GetProcessHeap, 0);
__declspec(dllimport) void *__stdcall
HeapAlloc(void *heap, ::std::uint_least32_t flags, ::std::size_t bytes) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(HeapAlloc, 12);
__declspec(dllimport) int __stdcall
HeapFree(void *heap, ::std::uint_least32_t flags, void *ptr) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(HeapFree, 12);
__declspec(dllimport) void *__stdcall LocalFree(void *mem) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(LocalFree, 4);
__declspec(dllimport) void __stdcall
RaiseException(::std::uint_least32_t exception_code,
               ::std::uint_least32_t exception_flags,
               ::std::uint_least32_t number_parameters,
               ::std::uintptr_t const *exception_information) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(RaiseException, 16);
__declspec(dllimport) ::std::uint_least32_t __stdcall
FormatMessageA(::std::uint_least32_t flags, void const *source,
               ::std::uint_least32_t message_id,
               ::std::uint_least32_t language_id, char *buffer,
               ::std::uint_least32_t size, char *arguments) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(FormatMessageA, 28);
__declspec(dllimport) ::std::uint_least32_t __stdcall
FormatMessageW(::std::uint_least32_t flags, void const *source,
               ::std::uint_least32_t message_id,
               ::std::uint_least32_t language_id, wchar_t *buffer,
               ::std::uint_least32_t size, char *arguments) noexcept
    __HERBCEPTIONS_WIN32_IMPORT(FormatMessageW, 28);

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
