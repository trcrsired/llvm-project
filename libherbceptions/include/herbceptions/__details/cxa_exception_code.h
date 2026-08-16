#pragma once
#include "../error"
#include <cstddef>

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
extern "C" __HERBCEPTIONS_CXA_CODE_API
::std::error_domain_singleton const* __cxa_error_domain_cxa_exception_code() noexcept;
#undef __HERBCEPTIONS_CXA_CODE_API

}

namespace std
{

// The code is a raw pointer to the thrown object (the __cxa catch value, i.e.
// the value __cxa_begin_catch() returns / what the landing-pad catch slot
// carries). The compiler fabricates it by magic when it captures a legacy C++
// exception at a `throws noexcept(false)` boundary. All legacy C++ exceptions
// share the same error domain; the type is NOT part of the domain identity.
//
// cxa_exception_code is opaque: it has no public members. Its sole purpose is
// to name the error_domain<cxa_exception_code> specialization so the compiler
// can fabricate {domain, code} values for captured legacy C++ exceptions. The
// stored representation is the thrown-object pointer, set by compiler magic.
class cxa_exception_code
{
    void* __cxa_exception_ptr{}; // no public members; compiler fabricates via magic
};

template<>
class error_domain<cxa_exception_code>
{
public:
    using errc_type = ::std::cxa_exception_code;
    static inline constexpr ::std::error_domain_singleton const* domain() noexcept
    {
        return ::std::error_domains::__cxa_error_domain_cxa_exception_code();
    }
    // The code of a cxa_exception_code value is the integer representation of
    // the thrown-object pointer the compiler stored in it. This function is
    // used by the compiler's fabrication machinery (and comparisons).
    static inline ::std::size_t code(errc_type __e) noexcept
    {
        ::std::size_t __temp;
        __builtin_memcpy(__builtin_addressof(__temp), __builtin_addressof(__e),
                         sizeof(::std::cxa_exception_code));
        return __temp;
    }
};

}
