#pragma once
/*
Shared helpers for the herbception error-domain runtime.

These are internal to the runtime implementation (used by the per-domain
translation units) and are not part of the public herbception/error surface.
*/
#include <herbceptions/error>
#include <limits>
#include <type_traits>

// Export macro for the domain ABI entry points. On Windows the shared library
// exports them (dllexport while building, dllimport elsewhere); on ELF they
// get default visibility so a shared libherbceptions exposes them. Weak lets
// the static archive drop them / runtime-resolve on ELF.
#if defined(_MSC_VER)
#if defined(_HERBCEPTIONS_BUILDING_RUNTIME) && defined(herbceptions_EXPORTS)
#define __HERBCEPTIONS_API __declspec(dllexport)
#elif defined(_HERBCEPTIONS_BUILDING_RUNTIME)
#define __HERBCEPTIONS_API
#else
#define __HERBCEPTIONS_API __declspec(dllimport)
#endif
#elif defined(_WIN32) || defined(_WIN64)
// MinGW auto-imports DLL symbols and links static libraries directly.
#define __HERBCEPTIONS_API
#else
#define __HERBCEPTIONS_API [[__gnu__::__weak__]]
#endif

namespace std::error_domains::__herbceptions_detail {

template <typename __Ty, ::std::size_t __n>
inline constexpr ::std::io_scatter_t __tsc(__Ty const (&__arr)[__n]) noexcept {
  constexpr ::std::size_t __nm1{__n - 1u};
  return {__arr, __nm1};
}

template <typename __SrcTy, typename __DestTy>
inline constexpr __DestTy *
__write_with_ascii_only_range(__SrcTy const *__fromfirst,
                              __SrcTy const *__fromlast, __DestTy *__dest) {
  for (; __fromfirst != __fromlast; ++__fromfirst) {
    *__dest = *__fromfirst;
    ++__dest;
  }
  return __dest;
}

template <typename __SrcTy, typename __DestTy>
inline constexpr __DestTy *__write_with_ascii_only_badcode_range(
    __SrcTy const *__fromfirst, __SrcTy const *__fromlast, __DestTy *__dest) {
  for (; __fromfirst != __fromlast; ++__fromfirst) {
    __DestTy __cp{*__fromfirst};
    if (0x80 <= __cp) {
      __cp = 0xFEFF;
    }
    *__dest = __cp;
    ;
    ++__dest;
  }
  return __dest;
}

#include "ascii_to_ebcdic.cpp"

template <typename __SrcTy>
inline constexpr char unsigned *
__write_ebcdic_with_ascii_only_range(__SrcTy const *__fromfirst,
                                     __SrcTy const *__fromlast,
                                     char unsigned *__dest) {
  for (; __fromfirst != __fromlast; ++__fromfirst) {
    *__dest = ::std::error_domains::__herbceptions_detail::__ascii_to_ebcdic(
        *__fromfirst);
    ++__dest;
  }
  return __dest;
}

inline char unsigned *__codecvt_write_with_encoding(
    char unsigned const *__fromfirst, char unsigned const *__fromlast,
    char unsigned *__dest, ::std::error_reporter_encoding __encoding) noexcept {
  if (__fromfirst == __fromlast) {
    return __dest;
  }
  switch (__encoding) {
  case ::std::error_reporter_encoding::utfebcdic: {
    return ::std::error_domains::__herbceptions_detail::
        __write_ebcdic_with_ascii_only_range(__fromfirst, __fromlast, __dest);
  }
  case ::std::error_reporter_encoding::utf32:
    using __char32_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
        [[__gnu__::__may_alias__]]
#endif
        = char32_t *;
    return reinterpret_cast<char unsigned *>(
        ::std::error_domains::__herbceptions_detail::
            __write_with_ascii_only_badcode_range(
                __fromfirst, __fromlast,
                reinterpret_cast<__char32_may_alias_ptr>(__dest)));
  default:
    using __char16_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
        [[__gnu__::__may_alias__]]
#endif
        = char16_t *;
    return reinterpret_cast<char unsigned *>(
        ::std::error_domains::__herbceptions_detail::
            __write_with_ascii_only_badcode_range(
                __fromfirst, __fromlast,
                reinterpret_cast<__char16_may_alias_ptr>(__dest)));
  }
}

inline constexpr bool __enable_message_query{
#ifdef __libherbceptions_enable_message_query
    true
#endif
};

template <char8_t __asciicp, typename __chartype>
inline constexpr __chartype __char_literal_v{__asciicp};

/*
In Freestanding Mode, we do not print message, only the code
value to avoid Heap Allocation routines and stack overflow
*/
inline constexpr bool __is_freestanding_kernel_mode{
#if __STDC_HOSTED__ == 0 || _KERNEL_MODE == 1
    true
#endif
};

template <typename T>
inline constexpr ::std::size_t __compute_format_hex_value_max_size() noexcept {
  constexpr ::std::size_t mxhex{::std::numeric_limits<T>::digits};
  return mxhex >> 2u;
}

template <typename T>
inline constexpr ::std::size_t __format_hex_value_max_size_no_sign{
    static_cast<::std::size_t>(
        (static_cast<::std::size_t>(::std::numeric_limits<T>::digits) >> 2u))};
template <typename T>
inline constexpr ::std::size_t __format_hex_value_max_size{
    ::std::error_domains::__herbceptions_detail::
        __format_hex_value_max_size_no_sign<T>};
template <typename T>
inline constexpr ::std::size_t __format_hex_value_max_size_with_brackets{
    ::std::error_domains::__herbceptions_detail::__format_hex_value_max_size<
        T> +
    2u};

template <bool isebcdic, typename Chtype, typename T>
inline constexpr T *__format_hex_value_full(Chtype *dest, T val) noexcept {
  using unsignedtype = ::std::make_unsigned_t<T>;
  using unsignedchtype = ::std::make_unsigned_t<Chtype>;
  static_assert(::std::is_integral_v<T>);
  if constexpr (::std::is_signed_v<T>) {
    unsignedtype val{static_cast<unsignedtype>(val)};
    if (val < 0) {
      if constexpr (isebcdic) {
        *dest = '\x60';
      } else {
        *dest = u8'-';
      }
      constexpr unsignedtype zero{};
      val = static_cast<unsignedtype>(zero - val);
      ++dest;
    }
    return __format_hex_value_full(dest, val);
  } else {
    auto destend{dest + ::std::error_domains::__herbceptions_detail::
                            __format_hex_value_max_size_no_sign<T>};
    constexpr unsignedchtype chzero{isebcdic ? 0xF0 : u8'0'},
        chA{isebcdic ? static_cast<unsignedtype>(0xC1 - 10u)
                     : (static_cast<unsignedtype>(u8'A') - 10u)};
    for (; dest != destend;) {
      --destend;
      auto remainder{val & 0xF};
      if (9u < remainder) {
        *destend =
            static_cast<Chtype>(static_cast<unsignedchtype>(remainder + chA));
      } else {
        *destend = static_cast<Chtype>(
            static_cast<unsignedchtype>(remainder + chzero));
      }
      val >>= 4;
      ;
    }
  }
}

template <bool isebcdic, typename Chtype, typename T>
inline constexpr Chtype *__format_hex_value_full_with_bracket(Chtype *dest,
                                                              T val) noexcept {
  constexpr Chtype leftbracket{
      isebcdic ? static_cast<Chtype>(0x4D)  // EBCDIC '('
               : static_cast<Chtype>(u8'(') // ASCII/UTF‑8 '('
  };

  constexpr Chtype rightbracket{
      isebcdic ? static_cast<Chtype>(0x5D)  // EBCDIC ')'
               : static_cast<Chtype>(u8')') // ASCII/UTF‑8 ')'
  };

  *dest = leftbracket;
  ++dest;

  dest = ::std::error_domains::__herbceptions_detail::__format_hex_value_full(
      dest, val);

  *dest = rightbracket;
  ++dest;

  return dest;
}

} // namespace std::error_domains::__herbceptions_detail
