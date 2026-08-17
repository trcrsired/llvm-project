//===--- itanium_exception_ptr.cpp - itanium exception_ptr domain --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the itanium exception_ptr error_domain_singleton vtable and the
// weak __cxa_error_domain_itanium_exception_ptr ABI entry point. Available on
// all platforms. The code is the thrown-object pointer (the __cxa catch
// value); the vtable releases it on cleanup and rethrows it via the ABI.
//
// The __cxa_* ABI (allocate/free/refcount/rethrow) exists only on Itanium-ABI
// targets (Linux, macOS, Cygwin, MinGW). The MSVC ABI (indicated by _MSC_VER)
// uses __CxxFrameHandler3/_CxxThrowException instead, so the vtable degrades
// gracefully there (no refcount/rethrow support).
//
//===----------------------------------------------------------------------===//

#include "__malloc_or_heap_alloc_temp_buffer.h"
#include "domain_helpers.h"
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <typeinfo>
#include <windows.h>
#undef min
#undef max

#if defined(_MSC_VER) && (defined(_WIN32) || defined(__CYGWIN__))

namespace std::error_domains::__details {
void __cdecl __ExceptionPtrDestroy(void *) noexcept
#if defined(__clang__) || defined(__GNUC__)
#if SIZE_MAX <= UINT_LEAST32_MAX &&                                            \
    (defined(__x86__) || defined(_M_IX86) || defined(__i386__))
#if !defined(__clang__)
    __asm__("?__ExceptionPtrDestroy@@YAXPAX@Z")
#else
    __asm__("?__ExceptionPtrDestroy@@YAXPAX@Z")
#endif
#else
    __asm__("?__ExceptionPtrDestroy@@YAXPEAX@Z")
#endif
#endif
        ;
void __cdecl __ExceptionPtrRethrow(void *)
#if defined(__clang__) || defined(__GNUC__)
#if SIZE_MAX <= UINT_LEAST32_MAX &&                                            \
    (defined(__x86__) || defined(_M_IX86) || defined(__i386__))
#if !defined(__clang__)
    __asm__("?__ExceptionPtrRethrow@@YAXPBX@Z")
#else
    __asm__("?__ExceptionPtrRethrow@@YAXPBX@Z")
#endif
#else
    __asm__("?__ExceptionPtrRethrow@@YAXPEBX@Z")
#endif
#endif
        ;

} // namespace std::error_domains::__details

namespace {

/*
Referenced from WINE code
*/

inline constexpr bool architecture_use_rva{
#if !defined(__i386__)
    true
#endif
};

inline uintptr_t cxx_rva_base(void const *ptr) noexcept {
  if constexpr (architecture_use_rva) {
    void *base;
    return reinterpret_cast<uintptr_t>(
        RtlPcToFileHeader(const_cast<void *>(ptr), __builtin_addressof(base)));
  } else {
    return reinterpret_cast<uintptr_t>(ptr);
  }
}
template <typename T>
inline void *cxx_rva(T rva, ::std::uintptr_t base) noexcept {
  if constexpr (architecture_use_rva) {
    return reinterpret_cast<void *>(base + rva);
  } else if constexpr (::std::is_pointer_v<T>) {
    return const_cast<void *>(rva);
  } else {
    return reinterpret_cast<void *>(static_cast<::std::uintptr_t>(rva));
  }
}

struct msvc_cppeh_info {
  void *obj;
  ::std::type_info *info;
};

struct msvc_cxx_exception_type {
  ::std::uint_least32_t flags;
  ::std::uint_least32_t destructor;
  ::std::uint_least32_t custom_handler;
  ::std::uint_least32_t type_info_table;
};
struct cxx_type_info_table {
  ::std::uint_least32_t count;
  ::std::uint_least32_t info[10];
};
struct cxx_type_info // RVA variant
{
  ::std::uint_least32_t flags;
  ::std::uint_least32_t type_info; // RVA to std::type_info
  int this_offset, vbase_descr, vbase_offset;
  ::std::uint_least32_t size;
  ::std::uint_least32_t copy_ctor;
};

struct msvc_raw_type_info {
  void *vtable;
  char *name;
  char mangled[128];
};

inline msvc_raw_type_info const *
get_msvc_cppeh_type_info(EXCEPTION_RECORD &ehrec) noexcept {
  auto *et = reinterpret_cast<msvc_cxx_exception_type *>(
      ehrec.ExceptionInformation[2]);
  uintptr_t base = cxx_rva_base(et);
  auto *table = reinterpret_cast<cxx_type_info_table *>(
      cxx_rva(et->type_info_table, base));
  auto *ti = reinterpret_cast<cxx_type_info *>(cxx_rva(table->info[0], base));
  return reinterpret_cast<msvc_raw_type_info const *>(
      cxx_rva(ti->type_info, base));
}
#if 0
inline void *get_this_pointer( const this_ptr_offsets *off, void *object )
{
    if (!object) return NULL;
    if (off->vbase_descr >= 0)
    {
        int *offset_ptr;
        /* move this ptr to vbase descriptor */
        object = (char *)object + off->vbase_descr;
        /* and fetch additional offset from vbase descriptor */
        offset_ptr = (int *)(*(char **)object + off->vbase_offset);
        object = (char *)object + *offset_ptr;
    }
    object = (char *)object + off->this_offset;
    return object;
}

inline ::std::exception* dynamic_cast_to_std_exception(EXCEPTION_RECORD const& ehrec) noexcept  
{  
    if (ehrec.ExceptionCode != 0xe06d7363 /* CXX_EXCEPTION */)  
        return nullptr;  
  
    unsigned nparams = architecture_use_rva ? 4 : 3;  
    if (ehrec.NumberParameters != nparams) return nullptr;  
    if (ehrec.ExceptionInformation[0] < 0x19930520 /* VC6 */ ||  
        ehrec.ExceptionInformation[0] > 0x19930520 /* adjust upper bound to real VC8 magic */)  
        return nullptr;  
  
    auto *et = reinterpret_cast<msvc_cxx_exception_type*>(ehrec.ExceptionInformation[2]);  
    uintptr_t base = architecture_use_rva ? ehrec.ExceptionInformation[3] : 0;  
    auto *table = reinterpret_cast<cxx_type_info_table*>(cxx_rva(et->type_info_table, base));  
  
    void *obj = reinterpret_cast<void*>(ehrec.ExceptionInformation[1]);  
  
    for (::std::uint_least32_t i{}; i != table->count; ++i)  
    {  
        auto *cti = reinterpret_cast<cxx_type_info*>(cxx_rva(table->info[i], base));  
        auto *except_ti = reinterpret_cast<msvc_raw_type_info*>(cxx_rva(cti->type_info, base));  
  
        if (!strcmp(except_ti->mangled, ".?AVexception@std@@"))  
        {  
            void *this_ptr = get_this_pointer(&cti->offsets, obj);  
            return reinterpret_cast<::std::exception*>(this_ptr);  
        }
    }  
    return nullptr;  
}
#endif
constinit ::std::error_domain_singleton __msvc_exception_ptr_domain{

    .do_cleanup =
        [](::std::size_t __cd) noexcept {
          void *__epstorage = reinterpret_cast<void *>(__cd);
          if (__epstorage == nullptr)
            return;
          ::std::error_domains::__details::__ExceptionPtrDestroy(__epstorage);
          ::std::error_domains::__herbceptions_detail::__free_or_heap_dealloc(
              __epstorage);
        },

    .do_equivalent = [](::std::size_t cd, ::std::error_domain_singleton const *,
                        ::std::size_t othercd) noexcept -> bool {
      return false;
    },

    .do_query_information =
        [](::std::size_t cd, ::std::error_query_information __query,
           ::std::error_reporter_encoding __encoding, void *__cookie,
           ::std::error_reporter_io_cookie_function __cookfun) noexcept {
          if (static_cast<::std::uint_least32_t>(
                  ::std::error_query_information::name_message) <
              static_cast<::std::uint_least32_t>(__query)) {
            return;
          }
          ::std::io_scatter_t __scatters[4];
          auto __pos{__scatters};
          EXCEPTION_RECORD &ehrec{**reinterpret_cast<EXCEPTION_RECORD **>(cd)};
          bool const ismsvccxxeh{ehrec.ExceptionCode == 0xe06d7363};
          if (ismsvccxxeh) // is msvc C++ ehcode
          {
            auto const *typeinfo{get_msvc_cppeh_type_info(ehrec)};
            char const *ehname{};
            ::std::size_t ehname_len{};
            if (::std::error_query_information::message != __query) {
              ehname = typeinfo->mangled;
              ehname_len = ::std::strlen(ehname);
            }
            char const *ehmessage{};
            ::std::size_t ehmessage_len{};
            if (::std::error_query_information::name != __query) {
#if 0
#endif
            }
            return;
          } else {
            if (::std::error_query_information::message == __query) {
              return;
            }
            switch (__query) {
            case ::std::error_query_information::name:
              switch (__encoding) {
              case ::std::error_reporter_encoding::utfebcdic: {
                *__pos = {"\x94\xA2\xA5\x83\x6D\x85\xA7\x83\x85\x97\xA3\x89\x96"
                          "\x95\x4D\x6F\x4D",
                          17u};
                break;
              }
              case ::std::error_reporter_encoding::utf16: {
                *__pos = {u"msvc_exception(?)", 17u * sizeof(char16_t)};
                break;
              }
              case ::std::error_reporter_encoding::utf32: {
                *__pos = {U"msvc_exception(?)", 17u * sizeof(char32_t)};
                break;
              }
              default: {
                *__pos = {u8"msvc_exception(?)", 17u};
                break;
              }
              }
              break;
            case ::std::error_query_information::name_message:
              switch (__encoding) {
              case ::std::error_reporter_encoding::utfebcdic: {
                *__pos = {"\xBA\x94\xA2\xA5\x83\x6D\x85\xA7\x83\x85\x97\xA3\x89"
                          "\x96\x95\x4D\x6F\x4D\xBB",
                          19u};
                break;
              }
              case ::std::error_reporter_encoding::utf16: {
                *__pos = {u"[msvc_exception(?)]", 19u * sizeof(char16_t)};
                break;
              }
              case ::std::error_reporter_encoding::utf32: {
                *__pos = {U"[msvc_exception(?)]", 19u * sizeof(char32_t)};
                break;
              }
              default: {
                *__pos = {u8"[msvc_exception(?)]", 19u};
                break;
              }
              }
              break;
            default:
              return;
            }
            ++__pos;
          }
          __cookfun(__cookie, __scatters,
                    static_cast<::std::size_t>(__pos - __scatters));
#if 0
      ::std::io_scatter_t __scatters[4];
      auto __pos{__scatters};



  
      bool ismsvccxxeh{ehrc.ExcetionCode == 0xe06d7363};
      if (ismsvccxxeh) //is msvc C++ ehcode
      {
        inline constexpr
#if 0
        if (::std::error_reporter_encoding::utf8 == encoding)
        {

        }
#endif
      }
      if (__query == ::std::error_query_information::name)
      {
        switch (encoding) {
        case ::std::error_reporter_encoding::utfebcdic: {
          *__pos = {"\x94\xA2\xA5\x83\x6D\x85\xA7\x83\x85\x97\xA3\x89\x96\x95", 15u};
          __pos[2] = {__builtin_addressof(::std::error_domains::__herbceptions_detail::__char_literal_v<u8')',char8_t>), sizeof(char32_t)};
          break;
        }
        case ::std::error_reporter_encoding::utf16: {
          *__pos = {u"msvc_exception(", 15u * sizeof(char16_t)};
          __pos[2] = {__builtin_addressof(::std::error_domains::__herbceptions_detail::__char_literal_v<u8')',char16_t>), sizeof(char16_t)};
          break;
        }
        case ::std::error_reporter_encoding::utf32: {
          *__pos = {U"msvc_exception(", 15u * sizeof(char32_t)};
          __pos[2] = {__builtin_addressof(::std::error_domains::__herbceptions_detail::__char_literal_v<u8')',char32_t>), sizeof(char32_t)};
          break;
        }
        default: {
          *__pos = {u8"msvc_exception(", 15u};
          __pos[2] = {__builtin_addressof(::std::error_domains::__herbceptions_detail::__char_literal_v<u8')',char8_t>), sizeof(char32_t)};
          break;
        }
        }
        ++__pos;
      }
      if (__query == ::std::error_query_information::name_message)
      {
        switch (encoding) {
        case ::std::error_reporter_encoding::utfebcdic: {
          *__pos = {"\xBA\x94\xA2\xA5\x83\x6D\x85\xA7\x83\x85\x97\xA3\x89\x96\x95\x4D", 16u};
          __pos[2] = {"\x4D\xBB", 2u};
          break;
        }
        case ::std::error_reporter_encoding::utf16: {
          *__pos = {u"[msvc_exception(", 16u * sizeof(char16_t)};
          __pos[2] = {U")]", 2u * sizeof(char16_t)};
          break;
        }
        case ::std::error_reporter_encoding::utf32: {
          *__pos = {U"[msvc_exception(", 16u * sizeof(char32_t)};
          __pos[2] = {U")]", 2u * sizeof(char32_t)};
          break;
        }
        default: {
          *__pos = {u8"[msvc_exception(", 16u};
          __pos[2] = {u8")]", 2u};
          break;
        }
        }
        ++__pos;
      }
      ::std::error_domains::__details::__malloc_or_heapalloc_temp_buffer __buffer;
      if (__query == ::std::error_query_information::name_message||
        __query == ::std::error_query_information::name)
      {
        
        __pos += 2;
      }
#endif
        },
    .do_to_errc = [](::std::size_t cd) noexcept -> ::std::errc {
      return ::std::errc::io_error;
    }
#if defined(__cpp_exceptions) && defined(_MSC_VER)
    ,
    .do_throw_dynamic_exception =
        [](::std::size_t __cd, ::std::dynamic_exception_abi __ehabi) {
          if (__ehabi != ::std::dynamic_exception_abi::platform)
            return;
          void *__epstorage = reinterpret_cast<void *>(__cd);
          if (__epstorage == nullptr)
            return;
          ::std::error_domains::__details::__ExceptionPtrRethrow(__epstorage);
        }
#endif
};
} // namespace

extern "C" __HERBCEPTIONS_API ::std::error_domain_singleton const *
__cxa_error_domain_msvc_exception_ptr() noexcept {
  return __builtin_addressof(__msvc_exception_ptr_domain);
}

#endif
