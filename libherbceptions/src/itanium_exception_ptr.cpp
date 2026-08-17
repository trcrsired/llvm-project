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

#include "domain_helpers.h"
#include "herbceptions/__details/exception_ptr.h"

#if defined(__cpp_exceptions) && !defined(_MSC_VER) &&                         \
    defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 1
#include <cstddef>
#include <exception>
#include <new>
#include <stdexcept>
#include <system_error>
#include <typeinfo>
#endif

namespace std::error_domains {
namespace {
using namespace __herbceptions_detail;

#if defined(__cpp_exceptions) && !defined(_MSC_VER)
// These are libc++abi symbols; libstdc++ hides them but libc++abi exports
// them. Declare them weak so the shared library links even when the C++ ABI
// library does not export them (they resolve at runtime when provided).
//
// On COFF (MinGW) weak extern references are not auto-imported from a DLL,
// so mark them dllimport there to import them from libc++abi.dll; on ELF the
// weak declaration lets them resolve at runtime (or stay null when absent).
#if defined(_WIN32) || defined(__CYGWIN__)
#define __HERBCEPTIONS_CXA_ABI __declspec(dllimport)
#else
#define __HERBCEPTIONS_CXA_ABI __attribute__((weak))
#endif
extern "C" __HERBCEPTIONS_CXA_ABI void
__cxa_decrement_exception_refcount(void *) noexcept;
extern "C" __HERBCEPTIONS_CXA_ABI void
__cxa_increment_exception_refcount(void *) noexcept;
extern "C" __HERBCEPTIONS_CXA_ABI void
__cxa_rethrow_primary_exception(void *) noexcept;
#undef __HERBCEPTIONS_CXA_ABI

// The __itanium_exception header precedes the thrown object in the same
// allocation ([header][thrown object]). Given the thrown-object pointer (the
// code / catch value), the header is at thrown_object - 1 (in units of the
// FULL __itanium_exception) and holds the std::type_info* of the dynamic type.
//
// The layout differs by ABI:
//   - LP64 Itanium (Linux, macOS, ...): _Unwind_Exception has two uintptr_t
//     private fields -> __itanium_exception is 128 bytes, exceptionType at +16.
//   - _WIN64 (MinGW): libc++abi builds with SEH, where _Unwind_Exception has
//     uintptr_t private_[6] (64 bytes) -> __itanium_exception is 160 bytes,
//     exceptionType still at +16.
// Only exceptionType's offset matters here, so branch on the target.
#if defined(_WIN64)
static constexpr ::std::ptrdiff_t __itanium_exception_size = 160;
#else
static constexpr ::std::ptrdiff_t __itanium_exception_size = 128;
#endif
static constexpr ::std::ptrdiff_t __itanium_exception_type_offset = 16;

struct __itanium_exception_layout {
  void *reserve;                             //  0
  ::std::size_t referenceCount;              //  8
  ::std::type_info *exceptionType;           // 16
  void (*exceptionDestructor)(void *);       // 24
  void (*unexpectedHandler)();               // 32
  void (*terminateHandler)();                // 40
  void *nextException;                       // 48
  int handlerCount;                          // 56
  int handlerSwitchValue;                    // 60
  unsigned char const *actionRecord;         // 64
  unsigned char const *languageSpecificData; // 72
  void *catchTemp;                           // 80
  void *adjustedPtr;                         // 88
  struct _Unwind_Exception {
    unsigned long long exception_class;     // 96
    void (*exception_cleanup)(int, void *); // 104
#if defined(_WIN64)
    ::std::uintptr_t private_[6]; // 112 .. 160 (SEH)
#else
    unsigned long private_1; // 112
    unsigned long private_2; // 120
#endif
  } unwindHeader;
};
static_assert(sizeof(__itanium_exception_layout) == __itanium_exception_size,
              "unexpected __itanium_exception layout");
constexpr ::std::type_info *cxa_type_info_of(::std::size_t cd) noexcept {
  if (cd == 0)
    return nullptr;
  unsigned char const *thrown = reinterpret_cast<unsigned char const *>(cd);
  unsigned char const *header =
      thrown - __itanium_exception_size + __itanium_exception_type_offset;
  return *reinterpret_cast<::std::type_info *const *>(header);
}

// Itanium ABI catch-matching primitive (the same one the personality routine
// uses): returns whether a handler of type T could catch the thrown object,
// storing the adjusted pointer in __obj.
//
// We must not reach into libc++abi's private __itanium_exception layout (the
// exceptionType offset differs between LP64 and SEH MinGW builds) beyond
// reading exceptionType, and libc++abi does not expose a public catch-match
// primitive. All the types checked here are standard exception types whose
// Itanium mangled names are fixed (St9exception, St12system_error, ...), so
// match the dynamic type's RTTI name against typeid(T).name() for exact
// subclass detection. Base-class detection (is this object any
// std::exception) is not needed: every std::exception subclass's what() and
// the errc mapping are obtained by matching the most-derived type name, which
// is what the personality would have used to pick a handler anyway.
template <typename T>
bool is_catchable_as(::std::size_t cd, void *&__obj) noexcept {
  ::std::type_info const *thrown = cxa_type_info_of(cd);
  if (!thrown)
    return false;
  if (*thrown != typeid(T))
    return false;
  __obj = reinterpret_cast<void *>(cd);
  return true;
}

// True if \p ti is (the RTTI of) a std::exception subclass. The names of the
// standard exception hierarchy in the Itanium mangling are fixed:
// std::exception itself mangles to St9exception and its subclasses to their
// own St*/NSt3__1* names, so a name in the std:: namespace with an
// exception-derived mangled name is treated as a std::exception. This is
// best-effort (user-defined exceptions deriving from std::exception are not
// matched), matching the best-effort nature of the message query.
bool is_std_exception(::std::type_info const *ti) noexcept {
  char const *name = ti->name();
  // Every std::exception subclass's mangled name starts with the std
  // namespace marker: 'S' + length (e.g. "St13runtime_error") or the
  // versioned "NSt3__1..." form.
  if (name[0] == 'S' && name[1] == 't')
    return true;
  if (name[0] == 'N' && name[1] == 'S' && name[2] == 't')
    return true;
  return false;
}

::std::errc errc_of(::std::error_code const &__ec) noexcept {
  if (__ec.category() == ::std::generic_category())
    return static_cast<::std::errc>(__ec.value());
  return ::std::errc::io_error;
}
#endif // __cpp_exceptions && !_MSC_VER

// Append "itanium_exception(<dynamic-type-name>)" into the pieces.
void append_cxa_name(query_information_pieces &pieces,
                     ::std::size_t cd) noexcept {
  pieces.add_cstr(u8"itanium_exception");
  pieces.add(reinterpret_cast<char8_t const *>("("), 1u);
#if defined(__cpp_exceptions) && !defined(_MSC_VER)
  ::std::type_info const *thrown = cxa_type_info_of(cd);
  if (thrown)
    pieces.add(reinterpret_cast<char8_t const *>(thrown->name()),
               __builtin_strlen(thrown->name()));
  else
    pieces.add(reinterpret_cast<char8_t const *>("?"), 1u);
#else
  pieces.add(reinterpret_cast<char8_t const *>("?"), 1u);
#endif
  pieces.add(reinterpret_cast<char8_t const *>(")"), 1u);
}

// Append the what() string into the pieces when the object is a
// std::exception.
void append_cxa_message(query_information_pieces &pieces,
                        ::std::size_t cd) noexcept {
#if defined(__cpp_exceptions) && !defined(_MSC_VER)
  ::std::type_info const *thrown = cxa_type_info_of(cd);
  if (thrown && is_std_exception(thrown)) {
    // The dynamic type is a std::exception subclass; std::exception is a
    // non-virtual base at offset 0, so the thrown-object pointer is the
    // std::exception subobject.
    ::std::exception const *e = reinterpret_cast<::std::exception const *>(cd);
    char const *what = e->what();
    pieces.add(reinterpret_cast<char8_t const *>(what), __builtin_strlen(what));
  }
#else
  (void)cd;
#endif
}

constinit ::std::error_domain_singleton __itanium_exception_ptr_domain{
    // The code is the thrown-object pointer (the __cxa catch value). When the
    // error value dies, release the reference; this destroys the exception
    // object exactly when the last reference goes away. On the MSVC ABI there
    // is no __cxa refcount, so cleanup is a no-op.
    .do_cleanup =
        [](::std::size_t cd) noexcept {
#if defined(__cpp_exceptions) && !defined(_MSC_VER)
          __cxa_decrement_exception_refcount(reinterpret_cast<void *>(cd));
#endif
        },
    // Two cxa exceptions are equivalent when they are the same exception
    // object (same catch value).
    .do_equivalent =
        [](::std::size_t cd, ::std::error_domain_singleton const *,
           ::std::size_t othercd) noexcept { return cd == othercd; },
    // The domain name is "itanium_exception", with the dynamic C++ type name
    // obtained through RTTI, e.g. "itanium_exception(std::runtime_error)". The
    // message is the what() string when the object is a std::exception.
    .do_query_information =
        [](::std::size_t cd, ::std::error_query_information query,
           ::std::error_reporter_encoding encoding, void *cookie,
           ::std::error_reporter_io_cookie_function cookfun) noexcept {
          query_information_pieces pieces;
          switch (query) {
          case ::std::error_query_information::name:
            append_cxa_name(pieces, cd);
            break;
          case ::std::error_query_information::message:
            append_cxa_message(pieces, cd);
            break;
          case ::std::error_query_information::name_message:
            append_cxa_name(pieces, cd);
            append_cxa_message(pieces, cd);
            break;
          }
          pieces.emit(encoding, cookie, cookfun);
        },
    .do_to_errc = [](::std::size_t cd) noexcept -> ::std::errc {
#if defined(__cpp_exceptions) && !defined(_MSC_VER)
      void *obj = nullptr;
      if (is_catchable_as<::std::system_error>(cd, obj))
        return static_cast<::std::errc>(
            errc_of(static_cast<::std::system_error const *>(obj)->code()));
      if (is_catchable_as<::std::bad_alloc>(cd, obj))
        return ::std::errc::not_enough_memory;
      if (is_catchable_as<::std::length_error>(cd, obj))
        return ::std::errc::value_too_large;
      if (is_catchable_as<::std::out_of_range>(cd, obj))
        return ::std::errc::result_out_of_range;
      if (is_catchable_as<::std::overflow_error>(cd, obj))
        return ::std::errc::value_too_large;
      if (is_catchable_as<::std::underflow_error>(cd, obj))
        return ::std::errc::result_out_of_range;
      if (is_catchable_as<::std::domain_error>(cd, obj))
        return ::std::errc::argument_out_of_domain;
      if (is_catchable_as<::std::invalid_argument>(cd, obj))
        return ::std::errc::invalid_argument;
      return ::std::errc::io_error;
#else
      (void)cd;
      return ::std::errc::io_error;
#endif
    }
// Rethrow the captured legacy C++ exception. On the MSVC ABI there is no
// __cxa_rethrow_primary_exception, so this is a no-op.
#if defined(__cpp_exceptions) && !defined(_MSC_VER)
    ,
    .do_throw_dynamic_exception =
        [](::std::size_t __cd, ::std::dynamic_exception_abi __ehabi) {
          if (__ehabi != ::std::dynamic_exception_abi::platform)
            return;
          __cxa_rethrow_primary_exception(reinterpret_cast<void *>(__cd));
        }
#endif
};
} // namespace

extern "C" __HERBCEPTIONS_API ::std::error_domain_singleton const *
__cxa_error_domain_itanium_exception_ptr() noexcept {
  return __builtin_addressof(
      ::std::error_domains::__itanium_exception_ptr_domain);
}

} // namespace std::error_domains
