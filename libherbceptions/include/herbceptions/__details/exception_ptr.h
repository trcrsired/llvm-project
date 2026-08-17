#pragma once
#include "../error"
#include <cstddef>
#include <exception>

namespace std::error_domains
{

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
extern "C" __HERBCEPTIONS_CXA_CODE_API
::std::error_domain_singleton const* __cxa_error_domain_msvc_exception_ptr() noexcept;
#else
extern "C" __HERBCEPTIONS_CXA_CODE_API
::std::error_domain_singleton const* __cxa_error_domain_itanium_exception_ptr() noexcept;
#endif
#undef __HERBCEPTIONS_CXA_CODE_API
#ifdef _MSC_VER
namespace __details
{

void* __cdecl __error_domains_msvc_current_exception() noexcept
#if defined(__clang__) || defined(__GNUC__)
#if SIZE_MAX <= UINT_LEAST32_MAX && (defined(__x86__) || defined(_M_IX86) || defined(__i386__))
#if !defined(__clang__)
	__asm__("__current_exception")
#else
	__asm__("___current_exception")
#endif
#else
	__asm__("__current_exception")
#endif
#endif
;

#if defined(_MSC_VER) && !defined(__clang__)

#endif

}
#endif

}

namespace std
{

template<>
class error_domain<::std::exception_ptr>
{
#ifdef _MSC_VER
    static inline ::std::size_t __builtin_herbceptions_exception_ptr_domain_msvc() noexcept
    {
        void* __ehptr = ::std::error_domains::__details::__error_domains_msvc_current_exception();
        return reinterpret_cast<::std::size_t>(__ehptr);
    }
#else
    static inline ::std::size_t __builtin_herbceptions_exception_ptr_domain_itanium(void* __e) noexcept
    {
        ::std::size_t __temp;
        __builtin_memcpy(__builtin_addressof(__temp), __builtin_addressof(__e),
                         sizeof(void*));
        return __temp;
    }
#endif
public:

    using errc_type = ::std::exception_ptr;
    static inline constexpr ::std::error_domain_singleton const* domain() noexcept
    {
#ifdef _MSC_VER
	    return ::std::error_domains::__cxa_error_domain_msvc_exception_ptr();
#else
        return ::std::error_domains::__cxa_error_domain_itanium_exception_ptr();
#endif
    }
    static inline ::std::size_t code(errc_type __e) noexcept
    {
        // Copy the first pointer-sized word of the exception_ptr. On Itanium
        // this is the thrown-object pointer; on MSVC it is the exception
        // record/object handle the domain understands.
        ::std::size_t __temp;
        __builtin_memcpy(__builtin_addressof(__temp), __builtin_addressof(__e),
                         sizeof(void*));
        return __temp;
    }
};

}
