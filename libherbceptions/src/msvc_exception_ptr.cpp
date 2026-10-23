//===--- msvc_exception_ptr.cpp - MSVC exception_ptr error domain --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the msvc exception_ptr error_domain_singleton vtable and the
// weak __cxa_error_domain_msvc_exception_ptr ABI entry point. The code is an
// MSVC std::exception_ptr box ({EXCEPTION_RECORD *, refcount *}) and the
// domain inspects the MSVC C++ EH record (0xE06D7363) it carries.
//
// _MSC_VER builds call the __ExceptionPtr* helpers imported from msvcp140.dll.
// GNU/MinGW builds get self-contained equivalents below, modeled on Wine's
// msvcrt and msvcp90: the box layout and process-heap allocation are ABI with
// msvcp140, so either runtime can release a box the other created. Rethrowing
// is a plain RaiseException; SEH delivers the record to the MSVC personalities
// above no matter which toolchain compiled the rethrowing module, so
// throw_legacy_exception works from -windows-gnu code too. That is why this
// translation unit keeps unwind tables although the rest of the runtime does
// not.
//
//===----------------------------------------------------------------------===//

// MSVC /kernel defines _KERNEL_MODE but leaves __STDC_HOSTED__ at 1; the
// exception_ptr box and the runtime imports it needs are user-mode only.
#if (defined(_WIN32) || defined(__CYGWIN__)) && __STDC_HOSTED__ == 1 &&        \
    !defined(_KERNEL_MODE)

#include "__malloc_or_heap_alloc_temp_buffer.h"
#include "libherbceptions.h"
#include "win32_imports.h"
#include <cstdlib>
#include <cstring>
#include <system_error>
#include <type_traits>
#include <typeinfo>

using ::std::error_domains::__herbceptions_detail::EXCEPTION_RECORD;
namespace win32 = ::std::error_domains::__herbceptions_detail::win32;

#if defined(_MSC_VER)
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

void __cdecl __ExceptionPtrCopy(void *, void const *) noexcept
#if defined(__clang__) || defined(__GNUC__)
#if SIZE_MAX <= UINT_LEAST32_MAX &&                                            \
    (defined(__x86__) || defined(_M_IX86) || defined(__i386__))
#if !defined(__clang__)
    __asm__("?__ExceptionPtrCopy@@YAXPAXPBX@Z")
#else
    __asm__("?__ExceptionPtrCopy@@YAXPAXPBX@Z")
#endif
#else
    __asm__("?__ExceptionPtrCopy@@YAXPEAXPEBX@Z")
#endif
#endif
        ;

void __cdecl __ExceptionPtrCopyException(void *, void const *,
                                         void const *) noexcept
#if defined(__clang__) || defined(__GNUC__)
#if SIZE_MAX <= UINT_LEAST32_MAX &&                                            \
    (defined(__x86__) || defined(_M_IX86) || defined(__i386__))
    __asm__("?__ExceptionPtrCopyException@@YAXPAXPBX1@Z")
#else
    __asm__("?__ExceptionPtrCopyException@@YAXPEAXPEBX1@Z")
#endif
#endif
        ;

} // namespace std::error_domains::__details
#endif

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
        ::std::error_domains::__herbceptions_detail::win32::RtlPcToFileHeader(
            const_cast<void *>(ptr), __builtin_addressof(base)));
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
get_msvc_cppeh_type_info(EXCEPTION_RECORD const &ehrec) noexcept {
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
      try_get_cpp_exception_with_mangled_name(ehrec, ".?AVexception@std@@")};
  if (this_ptr == nullptr) {
    // Older MSVC STLs kept the exception base in stdext.
    this_ptr = try_get_cpp_exception_with_mangled_name(
        ehrec, ".?AVexception@stdext@@");
  }
  if (this_ptr == nullptr)
    return nullptr;
  return get_msvc_exception_what(this_ptr);
}

// clang-format off
#include "msvc_exception_gperf"
// clang-format off

struct try_match_msvc_eh_result {
  msvc_exception_kind kind{};
  void *matched_obj{}; // filled for the kinds that need the subobject:
                       // system_error and cxx_std_error_legacy_exception
};

// Raw view of the std::_System_error prefix on x64 MSVC STL, used to
// extract the stored error_code without naming protected members:
//   exception      : vptr(8) + __std_exception_data{_What(8), _DoFree(1)+pad} =
//   24 runtime_error  : adds no members = 24 _System_error  : +
//   error_code{_Myval(4), pad, _Mycat(8)} => _Mycode @ 24
// std::system_error adds no data members over _System_error, so the same
// prefix holds for it; libc++'s std::__1/__2/__ndkN::system_error has the
// identical layout under the vcruntime ABI (only the decorated name in
// the throw record differs). sizeof is asserted as a layout tripwire.
struct msvc_system_error_raw {
  void *exception_prefix[3]; // vptr, _What, _DoFree+pad
  ::std::int_least32_t myval;
  void const *mycat;
};

// Returns true and fills \p val / \p cat when obj carries a generic-category
// error code.
inline bool
try_get_msvc_system_error_code(void const *obj, ::std::int_least32_t &val,
                               ::std::error_category const *&cat) noexcept {
  if (!obj) {
    return false;
  }
  auto *raw{static_cast<msvc_system_error_raw const *>(obj)};
  cat = static_cast<::std::error_category const *>(raw->mycat);
  // Comparing cat against std::generic_category() would drag the STL's
  // category singleton (vtable + message machinery) into this TU; its
  // name() is "generic" in every STL, which is good enough.
  if (!cat || ::std::strcmp(cat->name(), "generic") != 0) {
    return false; // custom category: no errc meaning
  }
  val = raw->myval;
  return true;
}

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
          kind == msvc_exception_kind::msvc__System_error ||
          kind == msvc_exception_kind::msvc_cxx_std_error_legacy_exception) {
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
  // "[msvc_exception](?)" (19 code units):
  //   startpos 0 len 19 -> "[msvc_exception](?)"
  //   startpos 0 len 17 -> "[msvc_exception](" (known prefix)
  //   startpos 0 len 16 -> "[msvc_exception]"
  //   startpos 1 len 14 -> "msvc_exception"
  //   startpos 16 len 1 -> "("
  //   startpos 18 len 1 -> ")"
  switch (encoding) {
#ifdef __LIBHERBCEPTIONS_ENABLE_EBCDIC
  case ::std::error_reporter_encoding::utfebcdic: {
    return {&startpos["\xBA\x94\xA2\xA5\x83\x6D\x85\xA7\x83\x85\x97\xA3\x89"
                      "\x96\x95\x5A\x4D\x6F\x5D"],
            n};
  }
#endif
  case ::std::error_reporter_encoding::utf16: {
    return {&startpos[u"[msvc_exception](?)"], n * sizeof(char16_t)};
  }
  case ::std::error_reporter_encoding::utf32: {
    return {&startpos[U"[msvc_exception](?)"], n * sizeof(char32_t)};
  }
  default: {
    return {&startpos[u8"[msvc_exception](?)"], n};
  }
  }
}

inline constexpr ::std::io_scatter_t
msvc_exception_name(::std::error_reporter_encoding encoding) noexcept {
  /*
  msvc_exception
  */
  return msvc_exception_name_message_range(encoding, 1, 14u);
}

inline constexpr ::std::io_scatter_t unknown_msvc_exception_name_message(
    ::std::error_reporter_encoding encoding) noexcept {
  /*
  [msvc_exception](?)
  */
  return msvc_exception_name_message_range(encoding, 0, 19u);
}

inline constexpr ::std::io_scatter_t known_msvc_exception_name_message_partial(
    ::std::error_reporter_encoding encoding) noexcept {
  /*
  [msvc_exception](
  */
  return msvc_exception_name_message_range(encoding, 0, 17u);
}

inline constexpr ::std::io_scatter_t unknown_msvc_exception_message_partial(
    ::std::error_reporter_encoding encoding) noexcept {
  /*
  (?)
  */
  return msvc_exception_name_message_range(encoding, 16, 3u);
}

inline constexpr ::std::io_scatter_t
left_parenthese(::std::error_reporter_encoding encoding) noexcept {
  /*
  (
  */
  return msvc_exception_name_message_range(encoding, 16, 1u);
}

inline constexpr ::std::io_scatter_t
right_parenthese(::std::error_reporter_encoding encoding) noexcept {
  /*
  )
  */
  return msvc_exception_name_message_range(encoding, 18, 1u);
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
  if (
#ifdef __LIBHERBCEPTIONS_ENABLE_EBCDIC
    encoding == ::std::error_reporter_encoding::utfebcdic ||
#endif
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

struct error_domain_msvc_eh_ptr {
  void *rec;
  void *ref;
};

/*
The two halves of the MSVC std::exception_ptr ABI used by this domain.
Wine's msvcrt and msvcp90 implement the same functions over EXCEPTION_RECORDs
with the same {record, refcount} box. Keeping their semantics (process-heap
boxes, Interlocked refcounts, throw-info driven copy and destruction) lets
msvcp140 and herbceptions share exception_ptr instances in either direction.
*/
#if defined(_MSC_VER)
inline void msvc_eh_ptr_destroy(void *p) noexcept {
  ::std::error_domains::__details::__ExceptionPtrDestroy(p);
}
inline void msvc_eh_ptr_copy(void *dst, void const *src) noexcept {
  ::std::error_domains::__details::__ExceptionPtrCopy(dst, src);
}
inline void msvc_eh_ptr_copy_exception(void *dst, void const *object,
                                       void const *type) noexcept {
  ::std::error_domains::__details::__ExceptionPtrCopyException(dst, object,
                                                               type);
}
inline void msvc_eh_ptr_current_exception(void *p) noexcept {
  ::std::error_domains::__details::__ExceptionPtrCurrentException(p);
}
inline void msvc_eh_ptr_rethrow(void *p) {
  ::std::error_domains::__details::__ExceptionPtrRethrow(p);
}
#else
inline void *msvc_eh_ptr_alloc(::std::size_t size) noexcept {
  return win32::HeapAlloc(win32::GetProcessHeap(), 0, size);
}
inline void msvc_eh_ptr_free(void *ptr) noexcept {
  if (ptr != nullptr)
    win32::HeapFree(win32::GetProcessHeap(), 0, ptr);
}
inline void msvc_eh_ptr_call_copy_ctor(void *fn, void *dest, void *src,
                                       bool has_virtual_base) noexcept {
#if defined(__i386__)
  if (has_virtual_base)
    reinterpret_cast<void (__thiscall *)(void *, void *, int)>(fn)(dest, src, 1);
  else
    reinterpret_cast<void (__thiscall *)(void *, void *)>(fn)(dest, src);
#else
  if (has_virtual_base)
    reinterpret_cast<void (*)(void *, void *, int)>(fn)(dest, src, 1);
  else
    reinterpret_cast<void (*)(void *, void *)>(fn)(dest, src);
#endif
}
inline void msvc_eh_ptr_call_dtor(void *fn, void *object) noexcept {
#if defined(__i386__)
  reinterpret_cast<void (__thiscall *)(void *)>(fn)(object);
#else
  reinterpret_cast<void (*)(void *)>(fn)(object);
#endif
}
inline void msvc_eh_ptr_copy_object(void *object, void *dest,
                                    cxx_type_info const *type,
                                    uintptr_t base) noexcept {
  if (type->flags & 0x1u /* CLASS_IS_SIMPLE_TYPE */) {
    __builtin_memmove(dest, object, type->size);
    if (type->size == sizeof(void *))
      *static_cast<void **>(dest) =
          get_this_pointer(&type->offsets, *static_cast<void **>(dest));
  } else if (type->copy_ctor) {
    msvc_eh_ptr_call_copy_ctor(
        cxx_rva(type->copy_ctor, base), dest,
        get_this_pointer(&type->offsets, object),
        (type->flags & 0x4u /* CLASS_HAS_VIRTUAL_BASE_CLASS */) != 0);
  } else {
    __builtin_memmove(dest, get_this_pointer(&type->offsets, object),
                      type->size);
  }
}
inline void msvc_eh_ptr_destruct_object(EXCEPTION_RECORD const &rec) noexcept {
  auto const *et = reinterpret_cast<msvc_cxx_exception_type const *>(
      rec.ExceptionInformation[2]);
  if (et == nullptr || et->destructor == 0)
    return;
  uintptr_t base = architecture_use_rva ? rec.ExceptionInformation[3] : 0;
  msvc_eh_ptr_call_dtor(cxx_rva(et->destructor, base),
                        reinterpret_cast<void *>(rec.ExceptionInformation[1]));
}
inline void msvc_eh_ptr_destroy(void *p) noexcept {
  auto *ep = static_cast<error_domain_msvc_eh_ptr *>(p);
  if (ep == nullptr || ep->rec == nullptr)
    return;
  if (__atomic_sub_fetch(static_cast<long *>(ep->ref), 1, __ATOMIC_SEQ_CST) ==
      0) {
    auto *rec = static_cast<EXCEPTION_RECORD *>(ep->rec);
    if (rec->ExceptionCode == 0xe06d7363u) {
      void *object = reinterpret_cast<void *>(rec->ExceptionInformation[1]);
      msvc_eh_ptr_destruct_object(*rec);
      msvc_eh_ptr_free(object);
    }
    msvc_eh_ptr_free(rec);
    msvc_eh_ptr_free(ep->ref);
  }
  ep->rec = nullptr;
  ep->ref = nullptr;
}
inline void msvc_eh_ptr_copy(void *dst, void const *src) noexcept {
  auto *d = static_cast<error_domain_msvc_eh_ptr *>(dst);
  auto const *s = static_cast<error_domain_msvc_eh_ptr const *>(src);
  *d = *s;
  if (d->ref != nullptr)
    __atomic_add_fetch(static_cast<long *>(d->ref), 1, __ATOMIC_SEQ_CST);
}
inline void msvc_eh_ptr_copy_exception(void *p, void const *object,
                                       void const *type_ptr) noexcept {
  auto *ep = static_cast<error_domain_msvc_eh_ptr *>(p);
  auto const *type = static_cast<msvc_cxx_exception_type const *>(type_ptr);
  uintptr_t base = cxx_rva_base(type);
  msvc_eh_ptr_destroy(ep);
  auto *rec = static_cast<EXCEPTION_RECORD *>(
      msvc_eh_ptr_alloc(sizeof(EXCEPTION_RECORD)));
  auto *ref = static_cast<long *>(msvc_eh_ptr_alloc(sizeof(long)));
  __builtin_memset(rec, 0, sizeof(*rec));
  rec->ExceptionCode = 0xe06d7363u;
  rec->ExceptionFlags = 0x1u /* EXCEPTION_NONCONTINUABLE */;
  rec->NumberParameters = architecture_use_rva ? 4u : 3u;
  rec->ExceptionInformation[0] = 0x19930520u /* VC6 frame magic */;
  rec->ExceptionInformation[2] = reinterpret_cast<::std::uintptr_t>(type);
  if constexpr (architecture_use_rva)
    rec->ExceptionInformation[3] = base;
  auto const *table = reinterpret_cast<cxx_type_info_table const *>(
      cxx_rva(type->type_info_table, base));
  auto const *ti =
      reinterpret_cast<cxx_type_info const *>(cxx_rva(table->info[0], base));
  void *copy = msvc_eh_ptr_alloc(ti->size);
  msvc_eh_ptr_copy_object(const_cast<void *>(object), copy, ti, base);
  rec->ExceptionInformation[1] = reinterpret_cast<::std::uintptr_t>(copy);
  *ref = 1;
  ep->rec = rec;
  ep->ref = ref;
}
/*
The CRT keeps the in-flight exception record in a per-thread slot exposed by
__current_exception(). A UCRT MinGW-w64 sysroot exports it from its import
library, so those builds bind it at link time. Legacy-msvcrt MinGW-w64 has no
such symbol in msvcrt.def.in, and Cygwin has no ucrtbase import, so they look
it up in DllMain at DLL_PROCESS_ATTACH, when every module the process needs is
already mapped. The lookup uses the same 9xa choice as the message text: the A
entry points where only they exist, W on NT. No such CRT means no MSVC
exception can be in flight, so asking for the current one is fatal.
*/
#if defined(_UCRT)
extern "C" __declspec(dllimport) void **__cdecl __current_exception() noexcept;
#else
using msvc_current_exception_fn = void **(__cdecl *)() noexcept;

using msvc_wide_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
    [[__gnu__::__may_alias__]]
#endif
    = wchar_t const *;

inline msvc_current_exception_fn __msvc_eh_ptr_current_exception_export =
    nullptr;

inline bool
msvc_eh_ptr_try_current_exception_module(void *module) noexcept {
  if (module == nullptr)
    return false;
  auto const fn = reinterpret_cast<msvc_current_exception_fn>(
      win32::GetProcAddress(module, reinterpret_cast<char const *>(
                                        u8"__current_exception")));
  if (fn == nullptr)
    return false;
  __msvc_eh_ptr_current_exception_export = fn;
  return true;
}

inline void msvc_eh_ptr_load_current_exception() noexcept {
  using ::std::error_domains::__herbceptions_detail::__win32_use_9xa_apis;
  if constexpr (__win32_use_9xa_apis) {
    char const *const modules[] = {
        reinterpret_cast<char const *>(u8"ucrtbase.dll"),
        reinterpret_cast<char const *>(u8"ucrtbased.dll"),
        reinterpret_cast<char const *>(u8"vcruntime140.dll"),
        reinterpret_cast<char const *>(u8"vcruntime140d.dll")};
    for (char const *module : modules) {
      if (msvc_eh_ptr_try_current_exception_module(
              win32::GetModuleHandleA(module)))
        return;
    }
  } else {
    char16_t const *const modules[] = {
        u"ucrtbase.dll", u"ucrtbased.dll", u"vcruntime140.dll",
        u"vcruntime140d.dll"};
    for (char16_t const *module : modules) {
      if (msvc_eh_ptr_try_current_exception_module(
              win32::GetModuleHandleW(reinterpret_cast<msvc_wide_ptr>(module))))
        return;
    }
  }
}
#endif
inline void msvc_eh_ptr_capture_record(void *p,
                                       EXCEPTION_RECORD const *src) noexcept {
  auto *ep = static_cast<error_domain_msvc_eh_ptr *>(p);
  if (src == nullptr) {
    ep->rec = nullptr;
    ep->ref = nullptr;
    return;
  }
  auto *rec = static_cast<EXCEPTION_RECORD *>(
      msvc_eh_ptr_alloc(sizeof(EXCEPTION_RECORD)));
  auto *ref = static_cast<long *>(msvc_eh_ptr_alloc(sizeof(long)));
  *rec = *src;
  *ref = 1;
  if (rec->ExceptionCode == 0xe06d7363u) {
    // Take ownership of a deep copy of the thrown object, exactly as a
    // throw site would: the exception_ptr releases it through the throw
    // info destructor.
    void *object = reinterpret_cast<void *>(rec->ExceptionInformation[1]);
    auto const *et = reinterpret_cast<msvc_cxx_exception_type const *>(
        reinterpret_cast<void *>(rec->ExceptionInformation[2]));
    uintptr_t base = cxx_rva_base(et);
    auto const *table = static_cast<cxx_type_info_table const *>(
        cxx_rva(et->type_info_table, base));
    auto const *ti =
        static_cast<cxx_type_info const *>(cxx_rva(table->info[0], base));
    void *copy = msvc_eh_ptr_alloc(ti->size);
    msvc_eh_ptr_copy_object(object, copy, ti, base);
    rec->ExceptionInformation[1] = reinterpret_cast<::std::uintptr_t>(copy);
  }
  ep->rec = rec;
  ep->ref = ref;
}
inline void msvc_eh_ptr_current_exception(void *p) noexcept {
#if defined(_UCRT)
  void **slot = __current_exception();
#else
  auto const fn = __msvc_eh_ptr_current_exception_export;
  if (fn == nullptr)
    ::std::abort();
  void **slot = fn();
#endif
  msvc_eh_ptr_capture_record(p, slot != nullptr
                                    ? static_cast<EXCEPTION_RECORD *>(*slot)
                                    : nullptr);
}
inline void msvc_eh_ptr_rethrow(void *p) {
  auto const *ep = static_cast<error_domain_msvc_eh_ptr const *>(p);
  if (ep == nullptr || ep->rec == nullptr)
    return; // empty: let throw_legacy_exception box its fallback
  auto const *rec = static_cast<EXCEPTION_RECORD const *>(ep->rec);
  win32::RaiseException(rec->ExceptionCode,
                        rec->ExceptionFlags & ~0x2u /* EXCEPTION_UNWINDING */,
                        rec->NumberParameters, rec->ExceptionInformation);
  // RaiseException only returns if no handler accepted the raise; crash
  // instead of falling off the end of a throw.
  __builtin_trap();
}
#endif

constexpr ::std::error_domain_singleton msvc_exception_ptr_domain{

    .do_cleanup =
        [](::std::size_t __cd) noexcept {
          void *__epstorage = reinterpret_cast<void *>(__cd);
          if (__epstorage == nullptr)
            return;
          msvc_eh_ptr_destroy(__epstorage);
          ::std::error_domains::__herbceptions_detail::__free_or_heap_dealloc(
              __epstorage);
        },

    .do_equivalent = [](::std::size_t cd,
                        ::std::error_domain_singleton const *domain,
                        ::std::size_t othercd) noexcept -> bool {
      if (cd) {
        EXCEPTION_RECORD &ehrec{**reinterpret_cast<EXCEPTION_RECORD **>(cd)};
        auto [kind, matched_obj] = try_match_msvc_exceptions(ehrec);
        if (kind ==
            msvc_exception_kind::msvc_cxx_std_error_legacy_exception) {
          // Our own box: equivalence is the carried domain's business.
          auto const *__box{static_cast<
              ::std::cxx_std_error_legacy_exception const *>(matched_obj)};
          auto *__inner{__box ? __box->domain() : nullptr};
          if (__inner == nullptr) {
            return false; // empty box equals nothing
          }
          if (__inner->do_equivalent != nullptr) {
            return __inner->do_equivalent(__box->code(), domain, othercd);
          }
          if (__inner->do_to_errc != nullptr) {
            return __inner->do_to_errc(__box->code()) ==
                   domain->do_to_errc(othercd);
          }
          return false;
        }
      }
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
          if constexpr (::std::error_domains::__herbceptions_detail::
                            __messages_disabled) {
            /*
            Messages disabled (freestanding, kernel mode, or __HERBCEPTIONS_NO_MESSAGES): no heap allocation and no big stack
            allocation. The mangled name and what() text are narrow (utf8 /
            gb18030 compatible) bytes that are already available in place,
            so they are only reported for those encodings; every other
            encoding would require a conversion buffer, so it degrades to
            (?) with no message.
            */
            bool const printable_encoding{
                encoding == ::std::error_reporter_encoding::utf8 ||
                encoding == ::std::error_reporter_encoding::gb18030};
            ::std::io_scatter_t scatters[4];
            ::std::size_t scatterlen{};
            // cd==0 means there is no current exception (the itanium sibling
            // reports "unknown" through the same path); never dereference it.
            EXCEPTION_RECORD const *ehrecptr{
                cd ? *reinterpret_cast<EXCEPTION_RECORD *const *>(cd)
                   : nullptr};
            bool const ismsvccxxeh{
                ehrecptr && ehrecptr->ExceptionCode == 0xe06d7363};
            if (ismsvccxxeh) {
              // Our own legacy-EH box: forward the report into the carried
              // domain so the original name/message survive the hop.
              auto [__kind, __obj]{try_match_msvc_exceptions(*ehrecptr)};
              if (__kind ==
                  msvc_exception_kind::msvc_cxx_std_error_legacy_exception) {
                auto const *__box{
                    static_cast<::std::cxx_std_error_legacy_exception const *>(
                        __obj)};
                if (auto *__inner{__box ? __box->domain() : nullptr};
                    __inner != nullptr &&
                    __inner->do_query_information != nullptr) {
                  __inner->do_query_information(__box->code(), query,
                                                encoding, cookie, cookfun);
                  return;
                }
              }
            }
            char const *ehname{};
            ::std::size_t ehname_len{};
            char const *ehmessage{};
            ::std::size_t ehmessage_len{};
            if (::std::error_query_information::name != query) {
              if (ismsvccxxeh) {
                auto const *typeinfo{get_msvc_cppeh_type_info(*ehrecptr)};
                ehname = typeinfo->mangled;
                if (ehname) {
                  ehname_len = ::std::strlen(ehname);
                }
                if (printable_encoding) {
                  ehmessage = try_get_std_exception_what(*ehrecptr);
                  if (ehmessage) {
                    ehmessage_len = ::std::strlen(ehmessage);
                  }
                }
              }
            }
            switch (static_cast<::std::uint_least32_t>(query)) {
            case static_cast<::std::uint_least32_t>(::std::error_query_information::name): {
              *scatters = msvc_exception_name(encoding);
              scatterlen = 1u;
              break;
            }
            case static_cast<::std::uint_least32_t>(::std::error_query_information::message):
              [[fallthrough]];
            case static_cast<::std::uint_least32_t>(::std::error_query_information::name_message): {
              if (::std::error_query_information::name_message == query) {
                if (printable_encoding && ehname_len) {
                  *scatters =
                      known_msvc_exception_name_message_partial(encoding);
                } else {
                  *scatters = unknown_msvc_exception_name_message(encoding);
                }
                ++scatterlen;
              }
              if (printable_encoding && ehname_len) {
                scatters[scatterlen] = left_parenthese(encoding);
                scatters[scatterlen + 1u] = {ehname, ehname_len};
                scatters[scatterlen + 2u] = right_parenthese(encoding);
                scatterlen += 3u;
              } else {
                scatters[scatterlen] =
                    unknown_msvc_exception_message_partial(encoding);
                ++scatterlen;
              }
              if (printable_encoding && ehmessage_len) {
                scatters[scatterlen] = {ehmessage, ehmessage_len};
                ++scatterlen;
              }
              break;
            }
            default: {
              return;
            }
            }
            cookfun(cookie, scatters, scatterlen);
          } else {
            ::std::error_domains::__herbceptions_detail::
                __malloc_or_heapalloc_temp_buffer buffer;
          ::std::io_scatter_t scatters[4];
          ::std::size_t scatterlen{};
          // cd==0 means there is no current exception (the itanium sibling
          // reports "unknown" through the same path); never dereference it.
          EXCEPTION_RECORD const *ehrecptr{
              cd ? *reinterpret_cast<EXCEPTION_RECORD *const *>(cd)
                 : nullptr};
          bool const ismsvccxxeh{
              ehrecptr && ehrecptr->ExceptionCode == 0xe06d7363};
          if (ismsvccxxeh) {
            // Our own legacy-EH box: forward the report into the carried
            // domain so the original name/message survive the hop.
            auto [__kind, __obj]{try_match_msvc_exceptions(*ehrecptr)};
            if (__kind ==
                msvc_exception_kind::msvc_cxx_std_error_legacy_exception) {
              auto const *__box{
                  static_cast<::std::cxx_std_error_legacy_exception const *>(
                      __obj)};
              if (auto *__inner{__box ? __box->domain() : nullptr};
                  __inner != nullptr &&
                  __inner->do_query_information != nullptr) {
                __inner->do_query_information(__box->code(), query, encoding,
                                              cookie, cookfun);
                return;
              }
            }
          }
          if (ismsvccxxeh) // is msvc C++ ehcode
          {
            auto const *typeinfo{get_msvc_cppeh_type_info(*ehrecptr)};
            char const *ehname{};
            ::std::size_t ehname_len{};
            if (::std::error_query_information::name != query) {
              ehname = typeinfo->mangled;
              if (ehname) {
                ehname_len = ::std::strlen(ehname);
              }
            }
            char const *ehmessage{};
            ::std::size_t ehmessage_len{};
            if (::std::error_query_information::name != query) {
              ehmessage = try_get_std_exception_what(*ehrecptr);
              if (ehmessage) {
                ehmessage_len = ::std::strlen(ehmessage);
              }
            }
            auto [name, message] = msvc_exception_writestr(
                ehname, ehname_len, ehmessage, ehmessage_len, encoding, buffer);
            switch (static_cast<::std::uint_least32_t>(query)) {
            case static_cast<::std::uint_least32_t>(::std::error_query_information::name): {
              *scatters = msvc_exception_name(encoding);
              scatterlen = 1u;
              break;
            }
            case static_cast<::std::uint_least32_t>(::std::error_query_information::message): {
              if (name.len) {
                *scatters = left_parenthese(encoding);
                scatters[1] = name;
                scatters[2] = right_parenthese(encoding);
                scatterlen = 3u;
              } else {
                *scatters = unknown_msvc_exception_message_partial(encoding);
                scatterlen = 1u;
              }
              if (message.len) {
                scatters[scatterlen] = message;
                ++scatterlen;
              }
              break;
            }
            case static_cast<::std::uint_least32_t>(::std::error_query_information::name_message): {
              if (name.len) {
                *scatters = known_msvc_exception_name_message_partial(encoding);
                scatters[1] = name;
                scatters[2] = right_parenthese(encoding);
                scatterlen = 3u;
              } else {
                *scatters = unknown_msvc_exception_name_message(encoding);
                scatterlen = 1u;
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
              *scatters = msvc_exception_name(encoding);
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
          }
        },
    .do_to_errc = [](::std::size_t cd) noexcept -> ::std::errc {
      if (!cd)
        return ::std::errc::io_error;
      EXCEPTION_RECORD &ehrec{**reinterpret_cast<EXCEPTION_RECORD **>(cd)};
      auto [kind, matched_obj] = try_match_msvc_exceptions(ehrec);
      switch (kind) {
      case msvc_exception_kind::msvc_cxx_std_error_legacy_exception: {
        // Our own legacy-EH box: the carried {domain, code} is the real
        // error, so its errc comes from the inner domain.
        if (auto const *__box{static_cast<
                ::std::cxx_std_error_legacy_exception const *>(matched_obj)};
            __box != nullptr) {
          if (auto *__inner{__box->domain()};
              __inner != nullptr && __inner->do_to_errc != nullptr) {
            return __inner->do_to_errc(__box->code());
          }
        }
        return ::std::errc::io_error;
      }
      case msvc_exception_kind::msvc_system_error:
        [[fallthrough]];
      case msvc_exception_kind::msvc__System_error:
        // std::system_error, its internal base _System_error, and the
        // libc++ spellings (std::__1/__2/__ndkN::system_error — same
        // {vptr, exception_data, error_code} layout under the vcruntime
        // ABI, only the decorated name differs) all store the error_code
        // at the same prefix offset; read it raw.
        {
          ::std::int_least32_t val{};
          ::std::error_category const *cat{};
          if (try_get_msvc_system_error_code(matched_obj, val, cat)) {
            return static_cast<::std::errc>(val);
          }
        }
        return ::std::errc::io_error;
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
    ,
    .do_throw_legacy_exception =
        [](::std::size_t __cd, ::std::legacy_exception_abi __ehabi) {
          /*
          Only an MSVC-ABI caller can catch what this domain raises: SEH
          hands the saved MSVC exception record to the MSVC personalities
          above regardless of which toolchain raised it, but an Itanium-ABI
          caller's typed handlers cannot.  The code belongs to the MSVC
          exception_ptr domain no matter which toolchain built this DLL, so
          compare against the MSVC ABI rather than the build platform.
          */
          if (__ehabi != ::std::legacy_exception_abi::microsoft)
            return;
          void *__epstorage = reinterpret_cast<void *>(__cd);
          if (__epstorage == nullptr)
            return;
          msvc_eh_ptr_rethrow(__epstorage);
        }
};
 
} // namespace

extern "C" __HERBCEPTIONS_API ::std::size_t
__cxa_error_code_msvc_exception_ptr(::std::size_t flags, void const *a,
                                    void const *b) noexcept {
  void *ehptr_storage = ::std::error_domains::__herbceptions_detail::
      __malloc_or_heap_alloc_or_die(sizeof(error_domain_msvc_eh_ptr));
  // The fresh box always starts as a valid empty exception_ptr:
  // msvc_eh_ptr_copy_exception releases whatever ep already holds, and a
  // zeroed box stays well-formed for cleanup on every path.
  __builtin_memset(ehptr_storage, 0, sizeof(error_domain_msvc_eh_ptr));
  switch (flags) {
  case 0:
    // a is an existing exception_ptr box; copy it into the fresh box.
    msvc_eh_ptr_copy(ehptr_storage, a);
    return reinterpret_cast<::std::size_t>(ehptr_storage);
  case 1:
    break;
  case 2:
    // Emitted by the herbceptions-legacy-eh-fold pass for a `throw` whose
    // unwind edge provably reaches only the legacy->std::error conversion
    // site: no exception is ever raised, so __current_exception has no
    // in-flight record to capture. a/b are the _CxxThrowException operands
    // (object, _ThrowInfo*).
    msvc_eh_ptr_copy_exception(ehptr_storage, a, b);
    return reinterpret_cast<::std::size_t>(ehptr_storage);
  default:
    ::std::abort();
  }
  msvc_eh_ptr_current_exception(ehptr_storage);
  return reinterpret_cast<::std::size_t>(ehptr_storage);
}

extern "C" __HERBCEPTIONS_API ::std::error_domain_singleton const *
__cxa_error_domain_msvc_exception_ptr() noexcept {
  return __builtin_addressof(msvc_exception_ptr_domain);
}

extern "C" __HERBCEPTIONS_API void
__cxa_error_code_msvc_exception_ptr_unbox(void *__epstorage,
                                          ::std::size_t __cd) noexcept {
  // __cd is a heap __std_exception_ptr box; msvc_eh_ptr_copy retains it
  // into the caller's exception_ptr storage.
  msvc_eh_ptr_copy(__epstorage, reinterpret_cast<void const *>(__cd));
}

extern "C" __HERBCEPTIONS_API void *__cxa_error_code_msvc_exception_ptr_cast(
    ::std::error_domain_singleton const *__domain, ::std::size_t __cd,
    void const *__ti) noexcept {
  if (__domain != __builtin_addressof(msvc_exception_ptr_domain) ||
      __cd == 0 || __ti == nullptr) {
    return nullptr;
  }
  EXCEPTION_RECORD &ehrec{**reinterpret_cast<EXCEPTION_RECORD **>(__cd)};
  // On the MSVC ABI a std::type_info IS the throw machinery's
  // _TypeDescriptor; the decorated name sits inline at the same offset as
  // msvc_raw_type_info::mangled, so no member call on the foreign type_info
  // is needed.
  auto const *__raw{reinterpret_cast<msvc_raw_type_info const *>(__ti)};
  return try_get_cpp_exception_with_mangled_name(ehrec, __raw->mangled);
}

#if !defined(_MSC_VER) && !defined(_UCRT)
/*
Legacy-msvcrt MinGW-w64 and Cygwin have no import for __current_exception, so
the DLL entry point resolves it once the loader has mapped everything. Running
here instead of in a namespace-scope constructor keeps the C++ runtime out of
the picture.
*/
extern "C" int __stdcall DllMain(void *, ::std::uint_least32_t reason,
                                 void *) noexcept {
  if (reason == 1u /* DLL_PROCESS_ATTACH */)
    msvc_eh_ptr_load_current_exception();
  return 1;
}
#endif

#endif
