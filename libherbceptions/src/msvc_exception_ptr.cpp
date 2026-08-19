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

#if defined(_MSC_VER) && (defined(_WIN32) || defined(__CYGWIN__))

#include "__malloc_or_heap_alloc_temp_buffer.h"
#include "domain_helpers.h"
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <typeinfo>
#include <windows.h>
#undef min
#undef max

namespace std::error_domains::__details {
void __cdecl __ExceptionPtrCurrentException(void *) noexcept
#if defined(__clang__) || defined(__GNUC__)
#if SIZE_MAX <= UINT_LEAST32_MAX &&                                            \
    (defined(__x86__) || defined(_M_IX86) || defined(__i386__))
#if !defined(__clang__)
    __asm__("?__ExceptionPtrCurrentException@@YAXPAX@Z")
#else
    __asm__("?__ExceptionPtrCurrentException@@YAXPAX@Z")
#endif
#else
    __asm__("?__ExceptionPtrCurrentException@@YAXPEAX@Z")
#endif
#endif
        ;

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
#if !((SIZE_MAX <= UINT_LEAST32_MAX) && (defined(__i386__) || defined(_M_IX86)))
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

struct this_ptr_offsets {
  ::std::int_least32_t
      this_offset; /* offset of base class this pointer from start of object */
  ::std::int_least32_t
      vbase_descr; /* offset of virtual base class descriptor */
  ::std::int_least32_t vbase_offset; /* offset of this pointer offset in virtual
                                        base class descriptor */
};
struct cxx_type_info // RVA variant
{
  ::std::uint_least32_t flags;
  ::std::uint_least32_t type_info; // RVA to std::type_info
  this_ptr_offsets offsets;
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

inline void *get_this_pointer(this_ptr_offsets const *off,
                              void *object) noexcept {
  if (!object)
    return nullptr;
  if (off->vbase_descr >= 0) {
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

inline char const *get_msvc_exception_what(void *obj) noexcept {
  using what_fn =
      char const *(__thiscall *)(void *) noexcept; // or __cdecl on x64
  void *vtbl = *reinterpret_cast<void **>(obj);
  return (reinterpret_cast<what_fn *>(vtbl)[1])(obj);
}

inline void *
try_get_cpp_exception_with_mangled_name(EXCEPTION_RECORD const &ehrec,
                                        char const *name) noexcept {
  if (ehrec.ExceptionCode != 0xe06d7363 /* CXX_EXCEPTION */)
    return nullptr;
  unsigned nparams = architecture_use_rva ? 4 : 3;
  if (ehrec.NumberParameters != nparams)
    return nullptr;
  if (ehrec.ExceptionInformation[0] < 0x19930520 /* VC6 */ ||
      ehrec.ExceptionInformation[0] >
          0x19930520 /* adjust upper bound to real VC8 magic */)
    return nullptr;

  auto *et = reinterpret_cast<msvc_cxx_exception_type *>(
      ehrec.ExceptionInformation[2]);
  uintptr_t base = architecture_use_rva ? ehrec.ExceptionInformation[3] : 0;
  auto *table = reinterpret_cast<cxx_type_info_table *>(
      cxx_rva(et->type_info_table, base));

  void *obj = reinterpret_cast<void *>(ehrec.ExceptionInformation[1]);

  for (::std::uint_least32_t i{}; i != table->count; ++i) {
    auto *cti =
        reinterpret_cast<cxx_type_info *>(cxx_rva(table->info[i], base));
    auto *except_ti =
        reinterpret_cast<msvc_raw_type_info *>(cxx_rva(cti->type_info, base));
    if (!strcmp(except_ti->mangled, name)) {
      return get_this_pointer(__builtin_addressof(cti->offsets), obj);
      // do not use C++ standard exception class since it can be libc++'s
    }
  }
  return nullptr;
}
// return nullptr if we do not find string
inline char const *
try_get_std_exception_what(EXCEPTION_RECORD const &ehrec) noexcept {
  auto this_ptr{
      try_get_cpp_exception_with_mangled_name(ehrec, ".?AVexception@stdext@@")};
  if (this_ptr == nullptr)
    return nullptr;
  return get_msvc_exception_what(this_ptr);
}

#include "msvc_exception_gperf"

struct try_match_msvc_eh_result {
  msvc_exception_kind kind{};
  void *system_error_obj{}; // we only catch system_error
};

inline try_match_msvc_eh_result
try_match_msvc_exceptions(EXCEPTION_RECORD const &ehrec) noexcept {
  if (ehrec.ExceptionCode != 0xe06d7363 /* CXX_EXCEPTION */)
    return {};
  unsigned nparams = architecture_use_rva ? 4 : 3;
  if (ehrec.NumberParameters != nparams)
    return {};
  if (ehrec.ExceptionInformation[0] < 0x19930520 /* VC6 */ ||
      ehrec.ExceptionInformation[0] >
          0x19930520 /* adjust upper bound to real VC8 magic */)
    return {};

  auto *et = reinterpret_cast<msvc_cxx_exception_type *>(
      ehrec.ExceptionInformation[2]);
  uintptr_t base = architecture_use_rva ? ehrec.ExceptionInformation[3] : 0;
  auto *table = reinterpret_cast<cxx_type_info_table *>(
      cxx_rva(et->type_info_table, base));

  void *obj = reinterpret_cast<void *>(ehrec.ExceptionInformation[1]);

  for (::std::uint_least32_t i{}; i != table->count; ++i) {
    auto *cti =
        reinterpret_cast<cxx_type_info *>(cxx_rva(table->info[i], base));
    auto *except_ti =
        reinterpret_cast<msvc_raw_type_info *>(cxx_rva(cti->type_info, base));
    char const *mangled_name{except_ti->mangled};
    ::std::size_t mangled_name_len{::std::strlen(mangled_name)};
    auto lookupres =
        Perfect_Hash::msvc_exception_lookup(mangled_name, mangled_name_len);
    if (lookupres) {
      auto kind{lookupres->value};
      if (kind == msvc_exception_kind::msvc_system_error ||
          kind == msvc_exception_kind::msvc__System_error) {
        return {kind, get_this_pointer(&cti->offsets, obj)};
      }
      return {kind};
    }
  }
  return {};
}

inline constexpr ::std::io_scatter_t
msvc_exception_name_message_range(::std::error_reporter_encoding encoding,
                                  ::std::size_t startpos,
                                  ::std::size_t n) noexcept {
  constexpr ::std::size_t totalsize{19u};
  switch (encoding) {
  case ::std::error_reporter_encoding::utfebcdic: {
    return {&startpos["\xBA\x94\xA2\xA5\x83\x6D\x85\xA7\x83\x85\x97\xA3\x89"
                      "\x96\x95\x4D\x6F\x4D\xBB"],
            n};
  }
  case ::std::error_reporter_encoding::utf16: {
    return {&startpos[u"[msvc_exception(?)]"], n * sizeof(char16_t)};
  }
  case ::std::error_reporter_encoding::utf32: {
    return {&startpos[U"[msvc_exception(?)]"], n * sizeof(char32_t)};
  }
  default: {
    return {&startpos[u8"[msvc_exception(?)]"], n};
  }
  }
}

inline constexpr ::std::io_scatter_t unknown_msvc_exception_name_message(
    ::std::error_reporter_encoding encoding) noexcept {
  /*
  [msvc_exception(?)]
  */
  return msvc_exception_name_message_range(encoding, 0, 19u);
}
inline constexpr ::std::io_scatter_t
unknown_msvc_exception_name(::std::error_reporter_encoding encoding) noexcept {
  /*
  msvc_exception(?)
  */
  return msvc_exception_name_message_range(encoding, 1, 17u);
}

inline constexpr ::std::io_scatter_t known_msvc_exception_name_partial(
    ::std::error_reporter_encoding encoding) noexcept {
  /*
  msvc_exception(
  */
  return msvc_exception_name_message_range(encoding, 1, 15u);
}

inline constexpr ::std::io_scatter_t known_msvc_exception_name_message_partial(
    ::std::error_reporter_encoding encoding) noexcept {
  /*
  [msvc_exception(
  */
  return msvc_exception_name_message_range(encoding, 0, 16u);
}

inline constexpr ::std::io_scatter_t
right_parenthese(::std::error_reporter_encoding encoding) noexcept {
  /*
  )
  */
  return msvc_exception_name_message_range(encoding, 17, 1u);
}

inline constexpr ::std::io_scatter_t
right_parenthese_bracket(::std::error_reporter_encoding encoding) noexcept {
  /*
  )]
  */
  return msvc_exception_name_message_range(encoding, 17, 2u);
}

struct msvc_exception_writestr_return {
  ::std::io_scatter_t name;
  ::std::io_scatter_t message;
};

inline constexpr msvc_exception_writestr_return msvc_exception_writestr(
    char const *name, ::std::size_t namelen, char const *message,
    ::std::size_t messagelen, ::std::error_reporter_encoding encoding,
    ::std::error_domains::__herbceptions_detail::
        __malloc_or_heapalloc_temp_buffer &buffer) noexcept {
  static_assert('A' == u8'A', "EBCDIC Execution Charset not supported");
  if (encoding == ::std::error_reporter_encoding::utfebcdic ||
      encoding == ::std::error_reporter_encoding::utf16 ||
      encoding == ::std::error_reporter_encoding::utf32) {
    ::std::size_t to_allocate_bytes{};
    if (__builtin_add_overflow(namelen, messagelen,
                               __builtin_addressof(to_allocate_bytes))) {
      abort();
    }
    if (to_allocate_bytes) {
      ::std::size_t szch{1u};
      if (encoding == ::std::error_reporter_encoding::utf16) {
        szch = 2u;
      } else if (encoding == ::std::error_reporter_encoding::utf32) {
        szch = 4u;
      }
      if (__builtin_mul_overflow(to_allocate_bytes, szch,
                                 __builtin_addressof(to_allocate_bytes))) {
        abort();
      }
      buffer.__bufferptr = ::std::error_domains::__herbceptions_detail::
          __malloc_or_heap_alloc_or_die(to_allocate_bytes);
      char unsigned *bufferptr{
          reinterpret_cast<char unsigned *>(buffer.__bufferptr)};
      auto name_end{
          ::std::error_domains::__herbceptions_detail::
              __codecvt_write_with_encoding(
                  reinterpret_cast<char unsigned const *>(name),
                  reinterpret_cast<char unsigned const *>(name + namelen),
                  bufferptr, encoding)};
      auto message_end{
          ::std::error_domains::__herbceptions_detail::
              __codecvt_write_with_encoding(
                  reinterpret_cast<char unsigned const *>(message),
                  reinterpret_cast<char unsigned const *>(message + messagelen),
                  name_end, encoding)};
      return {{bufferptr, static_cast<::std::size_t>(name_end - bufferptr)},
              {name_end, static_cast<::std::size_t>(message_end - name_end)}};
    }
  }
  return {{name, namelen}, {message, messagelen}};
}

constinit ::std::error_domain_singleton msvc_exception_ptr_domain{

    .do_cleanup =
        [](::std::size_t __cd) noexcept {
          void *__epstorage = reinterpret_cast<void *>(__cd);
          if (__epstorage == nullptr)
            return;
          ::std::error_domains::__details::__ExceptionPtrDestroy(__epstorage);
          ::std::error_domains::__herbceptions_detail::__free_or_heap_dealloc(
              __epstorage);
        },

    .do_equivalent = [](::std::size_t cd,
                        ::std::error_domain_singleton const *domain,
                        ::std::size_t othercd) noexcept -> bool {
      return msvc_exception_ptr_domain.do_to_errc(cd) ==
             domain->do_to_errc(othercd);
    },

    .do_query_information =
        [](::std::size_t cd, ::std::error_query_information query,
           ::std::error_reporter_encoding encoding, void *cookie,
           ::std::error_reporter_io_cookie_function cookfun) noexcept {
          if (static_cast<::std::uint_least32_t>(
                  ::std::error_query_information::name_message) <
              static_cast<::std::uint_least32_t>(query)) {
            return;
          }
          ::std::error_domains::__herbceptions_detail::
              __malloc_or_heapalloc_temp_buffer buffer;
          ::std::io_scatter_t scatters[4];
          ::std::size_t scatterlen{};
          EXCEPTION_RECORD &ehrec{**reinterpret_cast<EXCEPTION_RECORD **>(cd)};
          bool const ismsvccxxeh{ehrec.ExceptionCode == 0xe06d7363};
          if (ismsvccxxeh) // is msvc C++ ehcode
          {
            auto const *typeinfo{get_msvc_cppeh_type_info(ehrec)};
            char const *ehname{};
            ::std::size_t ehname_len{};
            if (::std::error_query_information::message != query) {
              ehname = typeinfo->mangled;
              if (ehname) {
                ehname_len = ::std::strlen(ehname);
              }
            }
            char const *ehmessage{};
            ::std::size_t ehmessage_len{};
            if (::std::error_query_information::name != query) {
              ehmessage = try_get_std_exception_what(ehrec);
              if (ehmessage) {
                ehmessage_len = ::std::strlen(ehmessage);
              }
            }
            auto [name, message] = msvc_exception_writestr(
                ehname, ehname_len, ehmessage, ehmessage_len, encoding, buffer);
            switch (query) {
            case ::std::error_query_information::name: {
              if (ehname) {
                *scatters = known_msvc_exception_name_partial(encoding);
                scatters[1] = name;
                scatters[2] = right_parenthese(encoding);
                scatterlen = 3u;
              } else {
                *scatters = unknown_msvc_exception_name(encoding);
                scatterlen = 1u;
              }
              break;
            }
            case ::std::error_query_information::message: {
              if (!message.len) {
                return;
              }
              *scatters = message;
              scatterlen = 1u;
              break;
            }
            case ::std::error_query_information::name_message: {
              if (name.len) {
                *scatters = known_msvc_exception_name_message_partial(encoding);
                scatters[1] = name;
                scatters[2] = right_parenthese_bracket(encoding);
                scatterlen = 3;
              } else {
                *scatters = unknown_msvc_exception_name_message(encoding);
                scatterlen = 1;
              }
              if (message.len) {
                scatters[scatterlen] = message;
                ++scatterlen;
              }
              break;
            }
            default: {
              return;
            }
            }
          } else {
            switch (query) {
            case ::std::error_query_information::name:
              *scatters = unknown_msvc_exception_name(encoding);
              break;
            case ::std::error_query_information::name_message:
              *scatters = unknown_msvc_exception_name_message(encoding);
              break;
            default:
              return;
            }
            scatterlen = 1u;
          }
          cookfun(cookie, scatters, scatterlen);
        },
    .do_to_errc = [](::std::size_t cd) noexcept -> ::std::errc {
      EXCEPTION_RECORD &ehrec{**reinterpret_cast<EXCEPTION_RECORD **>(cd)};
      auto [kind, system_error_obj] = try_match_msvc_exceptions(ehrec);
      switch (kind) {
#if 0
      case msvc_exception_kind::msvc_system_error:
        [[fallthrough]];
      case msvc_exception_kind::msvc__System_error:
        return ::std::errc::io_error;
#endif
      case msvc_exception_kind::msvc_logic_error:
        [[fallthrough]];
      case msvc_exception_kind::msvc_invalid_argument:
        return ::std::errc::invalid_argument;
      case msvc_exception_kind::msvc_domain_error:
        return ::std::errc::argument_out_of_domain;
      case msvc_exception_kind::msvc_length_error:
        return ::std::errc::value_too_large;
      case msvc_exception_kind::msvc_out_of_range:
        return ::std::errc::result_out_of_range;
      case msvc_exception_kind::msvc_overflow_error:
        [[fallthrough]];
      case msvc_exception_kind::msvc_underflow_error:
        return ::std::errc::value_too_large;
      case msvc_exception_kind::msvc_bad_alloc:
        return ::std::errc::not_enough_memory;
      default:
        return ::std::errc::io_error;
      }
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

extern "C" __HERBCEPTIONS_API ::std::size_t
__libherbceptions_exception_ptr_domain_msvc() noexcept {
  struct error_domain_msvc_eh_ptr {
    void *rec;
    void *ref;
  };
  void *ehptr_storage = ::std::error_domains::__herbceptions_detail::
      __malloc_or_heap_alloc_or_die(sizeof(error_domain_msvc_eh_ptr));
  if (ehptr_storage == nullptr)
    abort();
  ::std::error_domains::__details::__ExceptionPtrCurrentException(
      ehptr_storage);
  return reinterpret_cast<::std::size_t>(ehptr_storage);
}

extern "C" __HERBCEPTIONS_API ::std::error_domain_singleton const *
__cxa_error_domain_msvc_exception_ptr() noexcept {
  return __builtin_addressof(msvc_exception_ptr_domain);
}

#endif
