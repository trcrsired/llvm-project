#pragma once
#include "../error"
#include <cstddef>
#include <cstdlib>
#include <exception>

namespace std::error_domains {

#if defined(_MSC_VER)
#if defined(_HERBCEPTIONS_BUILDING_RUNTIME) && defined(herbceptions_EXPORTS)
#define __HERBCEPTIONS_CXA_CODE_API __declspec(dllexport)
#elif defined(_HERBCEPTIONS_BUILDING_RUNTIME)
#define __HERBCEPTIONS_CXA_CODE_API
#else
#define __HERBCEPTIONS_CXA_CODE_API __declspec(dllimport)
#endif
#elif defined(_WIN32) || defined(_WIN64)
// MinGW auto-imports DLL symbols and links static libraries directly.
#define __HERBCEPTIONS_CXA_CODE_API
#else
#define __HERBCEPTIONS_CXA_CODE_API [[__gnu__::__weak__]]
#endif
#ifdef _MSC_VER
extern "C" __HERBCEPTIONS_CXA_CODE_API ::std::error_domain_singleton const *
__cxa_error_domain_msvc_exception_ptr() noexcept;
#else
extern "C" __HERBCEPTIONS_CXA_CODE_API ::std::error_domain_singleton const *
__cxa_error_domain_itanium_exception_ptr() noexcept;
#endif
#undef __HERBCEPTIONS_CXA_CODE_API
#ifdef _MSC_VER
namespace __details {

void __cdecl __error_domains___ExceptionPtrCurrentException(void *) noexcept
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
#if defined(_WIN32) || defined(__CYGWIN__)
struct __error_domain_msvc_eh_ptr {
  void *__error_domain_rec;
  void *__error_domain_ref;
};
#endif

} // namespace __details
#endif

} // namespace std::error_domains

namespace std {

template <> class error_domain<::std::exception_ptr> {
#ifdef _MSC_VER
  static inline ::std::size_t
  __builtin_herbceptions_exception_ptr_domain_msvc() noexcept {
    void *__ehptr_storage =
        malloc(sizeof(::std::error_domains::__details::
                          __error_domain_msvc_eh_ptr)); // assume malloc exists
    if (__ehptr_storage == nullptr)
      abort();
    ::std::error_domains::__details::
        __error_domains___ExceptionPtrCurrentException(__ehptr_storage);
    return reinterpret_cast<::std::size_t>(__ehptr_storage);
  }
#else
  static inline ::std::size_t
  __builtin_herbceptions_exception_ptr_domain_itanium(void *__e) noexcept {
    ::std::size_t __temp;
    __builtin_memcpy(__builtin_addressof(__temp), __builtin_addressof(__e),
                     sizeof(void *));
    return __temp;
  }
#endif
public:
  using errc_type = ::std::exception_ptr;
  static inline constexpr ::std::error_domain_singleton const *
  domain() noexcept {
#ifdef _MSC_VER
    return ::std::error_domains::__cxa_error_domain_msvc_exception_ptr();
#else
    return ::std::error_domains::__cxa_error_domain_itanium_exception_ptr();
#endif
  }
  static inline ::std::size_t code(errc_type __e) noexcept {
    // Copy the first pointer-sized word of the exception_ptr. On Itanium
    // this is the thrown-object pointer; on MSVC it is the exception
    // record/object handle the domain understands.
    ::std::size_t __temp;
    __builtin_memcpy(__builtin_addressof(__temp), __builtin_addressof(__e),
                     sizeof(void *));
    return __temp;
  }
};

} // namespace std
