#pragma once
/*
com (com_errc) error domain header.

Declares com_errc (HRESULT values) and its error_domain specialization. The
singleton vtable is implemented in src/com.cpp. Only available on
_WIN32/__CYGWIN__ targets.
*/

#if defined(_WIN32) || defined(__CYGWIN__)

namespace std {

// The underlying type is uint32 so the standard 0x8000xxxx "failure" bit
// pattern fits without narrowing.
enum class com_errc : ::std::uint_least32_t {
  ok = 0x00000000,
  nointerface = 0x80004002,
  fail = 0x80004005,
  unexpected = 0x8000FFFF,
  notimpl = 0x80004001,
  outofmemory = 0x8007000E,
  invalidarg = 0x80070057,
  accessdenied = 0x80070005,
  handle = 0x80070006,
  abort = 0x80004004,
  fail_ie = 0x8000FFFD,
  pending = 0x8000000A,
  cancelled = 0x800704C7,
  notfound = 0x80070490,
  alreadyexists = 0x800700B7,
  nointerface_ie = 0x8000FFFE
};

template <> class error_domain<::std::com_errc> {
public:
  using errc_type = ::std::com_errc;
  static inline constexpr ::std::error_domain_singleton const *
  domain() noexcept {
    return ::std::error_domains::__cxa_error_domain_com();
  }
  static inline constexpr ::std::size_t code(errc_type __e) noexcept {
    return static_cast<::std::size_t>(static_cast<::std::uint_least32_t>(__e));
  }
};

} // namespace std

#endif // _WIN32 || __CYGWIN__
