#pragma once

#include <cxxabi.h>
#include <unwind.h>

#if defined(__arm__) && !defined(__USING_SJLJ_EXCEPTIONS__) &&                 \
    !defined(__ARM_DWARF_EH__) && !defined(__SEH__)
#define _LIBHERBCEPTIONS_ARM_EHABI
#endif

#if __has_feature(ptrauth_calls)

// ptrauth_string_discriminator("__cxa_exception::actionRecord") == 0xFC91
#define __ptrauth_libherbceptions_action_record                                \
  __ptrauth(ptrauth_key_process_dependent_data, 1, 0xFC91)

// ptrauth_string_discriminator("__cxa_exception::languageSpecificData") ==
// 0xE8EE
#define __ptrauth_libherbceptions_lsd                                          \
  __ptrauth(ptrauth_key_process_dependent_data, 1, 0xE8EE)

// ptrauth_string_discriminator("__cxa_exception::catchTemp") == 0xFA58
#define __ptrauth_libherbceptions_catch_temp_disc 0xFA58
#define __ptrauth_libherbceptions_catch_temp_key                               \
  ptrauth_key_process_dependent_data
#define __ptrauth_libherbceptions_catch_temp                                   \
  __ptrauth(__ptrauth_libherbceptions_catch_temp_key, 1,                       \
            __ptrauth_libherbceptions_catch_temp_disc)

// ptrauth_string_discriminator("__cxa_exception::adjustedPtr") == 0x99E4
#define __ptrauth_libherbceptions_adjusted_ptr                                 \
  __ptrauth(ptrauth_key_process_dependent_data, 1, 0x99E4)

// ptrauth_string_discriminator("__cxa_exception::unexpectedHandler") == 0x99A9
#define __ptrauth_libherbceptions_unexpected_handler                           \
  __ptrauth(ptrauth_key_function_pointer, 1, 0x99A9)

// ptrauth_string_discriminator("__cxa_exception::terminateHandler") == 0x0886)
#define __ptrauth_libherbceptions_terminate_handler                            \
  __ptrauth(ptrauth_key_function_pointer, 1, 0x886)

// ptrauth_string_discriminator("__cxa_exception::exceptionDestructor") ==
// 0xC088
#define __ptrauth_libherbceptions_exception_destructor                         \
  __ptrauth(ptrauth_key_function_pointer, 1, 0xC088)

#else

#define __ptrauth_libherbceptions_action_record
#define __ptrauth_libherbceptions_lsd
#define __ptrauth_libherbceptions_catch_temp
#define __ptrauth_libherbceptions_adjusted_ptr
#define __ptrauth_libherbceptions_unexpected_handler
#define __ptrauth_libherbceptions_terminate_handler
#define __ptrauth_libherbceptions_exception_destructor

#endif

#if defined(_WIN32)
#define _LIBHERBCEPTIONS_DTOR_FUNC __thiscall
#else
#define _LIBHERBCEPTIONS_DTOR_FUNC
#endif

namespace {
struct itanium_cxa_exception {
#if defined(__LP64__) || defined(_WIN64) || defined(_LIBHERBCEPTIONS_ARM_EHABI)
  // Now _Unwind_Exception is marked with __attribute__((aligned)),
  // which implies __cxa_exception is also aligned. Insert padding
  // in the beginning of the struct, rather than before unwindHeader.
  void *reserve;

  // This is a new field to support C++11 exception_ptr.
  // For binary compatibility it is at the start of this
  // struct which is prepended to the object thrown in
  // __cxa_allocate_exception.
  size_t referenceCount;
#endif

  //  Manage the exception object itself.
  std::type_info *exceptionType;
#ifdef __wasm__
  // In Wasm, a destructor returns its argument
  void *(_LIBHERBCEPTIONS_DTOR_FUNC *exceptionDestructor)(void *);
#else
  void(
      _LIBHERBCEPTIONS_DTOR_FUNC *__ptrauth_libherbceptions_exception_destructor
          exceptionDestructor)(void *);
#endif
  void *__ptrauth_libherbceptions_unexpected_handler unexpectedHandler;
  void *__ptrauth_libherbceptions_terminate_handler terminateHandler;

  itanium_cxa_exception *nextException;

  int handlerCount;

#if defined(_LIBHERBCEPTIONS_ARM_EHABI)
  itanium_cxa_exception *nextPropagatingException;
  int propagationCount;
#else
  int handlerSwitchValue;
  const unsigned char *actionRecord;
  const unsigned char *languageSpecificData;
  void *__ptrauth_libherbceptions_catch_temp catchTemp;
  void *__ptrauth_libherbceptions_adjusted_ptr adjustedPtr;
#endif

#if !defined(__LP64__) && !defined(_WIN64) &&                                  \
    !defined(_LIBHERBCEPTIONS_ARM_EHABI)
  // This is a new field to support C++11 exception_ptr.
  // For binary compatibility it is placed where the compiler
  // previously added padding to 64-bit align unwindHeader.
  size_t referenceCount;
#endif

  _Unwind_Exception unwindHeader;
};

struct itanium_cxa_dependent_exception {
#if defined(__LP64__) || defined(_WIN64) || defined(_LIBHERBCEPTIONS_ARM_EHABI)
  void *reserve; // padding.
  void *primaryException;
#endif

  std::type_info *exceptionType;
  void(
      _LIBHERBCEPTIONS_DTOR_FUNC *__ptrauth_libherbceptions_exception_destructor
          exceptionDestructor)(void *);
  void *__ptrauth_libherbceptions_unexpected_handler unexpectedHandler;
  std::terminate_handler __ptrauth_libherbceptions_terminate_handler
      terminateHandler;

  itanium_cxa_exception *nextException;

  int handlerCount;

#if defined(_LIBHERBCEPTIONS_ARM_EHABI)
  itanium_cxa_exception *nextPropagatingException;
  int propagationCount;
#else
  int handlerSwitchValue;
  const unsigned char *__ptrauth_libherbceptions_action_record actionRecord;
  const unsigned char *__ptrauth_libherbceptions_lsd languageSpecificData;
  void *__ptrauth_libherbceptions_catch_temp catchTemp;
  void *__ptrauth_libherbceptions_adjusted_ptr adjustedPtr;
#endif

#if !defined(__LP64__) && !defined(_WIN64) &&                                  \
    !defined(_LIBHERBCEPTIONS_ARM_EHABI)
  void *primaryException;
#endif
  _Unwind_Exception unwindHeader;
};

} // namespace

#ifdef _LIBCPPABI_VERSION

extern "C" __HERBCEPTIONS_API ::std::size_t
__libherbceptions_exception_ptr_domain_itanium(void *eh) noexcept {
  ::__cxxabiv1::__cxa_increment_exception_refcount(eh);
  return reinterpret_cast<::std::size_t>(eh);
}

namespace {
inline void
__itanium_cxa_decrement_exception_refcount(void *thrown_object) noexcept {
  ::__cxxabiv1::__cxa_decrement_exception_refcount(thrown_object);
}
} // namespace

#else

#if __cplusplus >= 201103L
namespace std {
void *get_unexpected() noexcept;
}
#endif
namespace {

inline itanium_cxa_exception *
itanium_cxa_exception_from_thrown_object(void *thrown_object) noexcept {
  return static_cast<itanium_cxa_exception *>(thrown_object) - 1;
}

inline void
itanium_cxa_increment_exception_refcount(void *thrown_object) noexcept {
  if (thrown_object == nullptr) {
    return;
  }
  __atomic_add_fetch(__builtin_addressof(
                         itanium_cxa_exception_from_thrown_object(thrown_object)
                             ->referenceCount),
                     1u, __ATOMIC_SEQ_CST);
}

inline void
itanium_cxa_decrement_exception_refcount(void *thrown_object) noexcept {
  if (thrown_object == nullptr) {
    return;
  }
  auto exception_header{
      itanium_cxa_exception_from_thrown_object(thrown_object)};
  size_t &referenceCount{exception_header->referenceCount};
  if (!__atomic_add_fetch(__builtin_addressof(referenceCount),
                          static_cast<::std::size_t>(-1), __ATOMIC_SEQ_CST)) {
    if (nullptr != exception_header->exceptionDestructor)
      exception_header->exceptionDestructor(thrown_object);
    ::__cxxabiv1::__cxa_free_exception(thrown_object);
  }
}

#if 0

inline void setDependentExceptionClass(_Unwind_Exception* unwind_exception) noexcept {
    unwind_exception->exception_class
    = ((((((((_Unwind_Exception_Class) 'G'
        << 8 | (_Unwind_Exception_Class) 'N')
        << 8 | (_Unwind_Exception_Class) 'U')
        << 8 | (_Unwind_Exception_Class) 'C')
        << 8 | (_Unwind_Exception_Class) 'C')
        << 8 | (_Unwind_Exception_Class) '+')
        << 8 | (_Unwind_Exception_Class) '+')
    << 8 | (_Unwind_Exception_Class) '\x01');
}

inline
void __itanium_cxa_rethrow_primary_exception(void* thrown_object)
{
    if ( thrown_object != NULL )
    {
        // thrown_object guaranteed to be native because
        //   __cxa_current_primary_exception returns NULL for foreign exceptions
        auto exception_header = itanium_cxa_exception_from_thrown_object(thrown_object);
        itanium_cxa_dependent_exception* dep_exception_header =
            reinterpret_cast<itanium_cxa_dependent_exception*>(__cxxabiv1::__cxa_allocate_dependent_exception());
        dep_exception_header->primaryException = thrown_object;
        itanium_cxa_increment_exception_refcount(thrown_object);
        dep_exception_header->exceptionType = exception_header->exceptionType;
        dep_exception_header->unexpectedHandler = ::std::get_unexpected();
        dep_exception_header->terminateHandler = ::std::get_terminate();
        setDependentExceptionClass(&dep_exception_header->unwindHeader);
        ::__cxxabiv1::__cxa_get_globals()->uncaughtExceptions += 1;
        dep_exception_header->unwindHeader.exception_cleanup = dependent_exception_cleanup;
#ifdef __USING_SJLJ_EXCEPTIONS__
        _Unwind_SjLj_RaiseException(&dep_exception_header->unwindHeader);
#else
        _Unwind_RaiseException(&dep_exception_header->unwindHeader);
#endif
        // Some sort of unwinding error.  Note that terminate is a handler.
        ::__cxxabiv1::__cxa_begin_catch(&dep_exception_header->unwindHeader);
    }
    // If we return client will call terminate()
}
#endif
} // namespace

extern "C" __HERBCEPTIONS_API ::std::size_t
__cxa_error_code_itanium_exception_ptr(void *eh) noexcept {
  itanium_cxa_increment_exception_refcount(eh);
  return reinterpret_cast<::std::size_t>(eh);
}

#endif
