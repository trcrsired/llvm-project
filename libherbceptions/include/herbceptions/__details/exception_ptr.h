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
extern "C" __HERBCEPTIONS_CXA_CODE_API ::std::size_t
__libherbceptions_exception_ptr_domain_msvc() noexcept;
#else
extern "C" __HERBCEPTIONS_CXA_CODE_API ::std::error_domain_singleton const *
__cxa_error_domain_itanium_exception_ptr() noexcept;
extern "C" ::std::size_t
__libherbceptions_exception_ptr_domain_itanium(void *) noexcept;
#endif
#undef __HERBCEPTIONS_CXA_CODE_API
} // namespace std::error_domains

namespace std {

template <> class error_domain<::std::exception_ptr> {
#ifdef _MSC_VER
  static inline ::std::size_t
  __builtin_herbceptions_exception_ptr_domain_msvc() noexcept {
    return ::std::error_domains::__libherbceptions_exception_ptr_domain_msvc();
  }
#else
  static inline ::std::size_t
  __builtin_herbceptions_exception_ptr_domain_itanium(void *__ehptr) noexcept {
    return ::std::error_domains::__libherbceptions_exception_ptr_domain_itanium(
        __ehptr);
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
