#pragma once
/*
Cross-domain mapping tables for the nt, win32 and com error domains
(private).

All lookup data lives in generated switch fragments (utils/ntkernel-table.json
is the source of truth, consumed by the generator that emits the .hpp files):
any nonzero NTSTATUS is treated as an error regardless of its severity bits.

The message switch is only ever referenced from hosted builds - freestanding
kernel translation units never emit it.
*/
#include <cstddef>
#include <cstdint>
#include <herbceptions/error>

namespace std::error_domains {
namespace __herbceptions_detail {

/*
COM HRESULT classification shared by the com, nt and win32 domains'
cross-domain equivalence rules.
*/
inline constexpr ::std::uint_least32_t com_facility_nt_bit{0x10000000u};
inline constexpr ::std::uint_least32_t com_facility_win32{7u};
inline constexpr ::std::uint_least32_t com_hresult_code_mask{0xFFFFu};
inline constexpr ::std::uint_least32_t
com_hresult_facility(::std::uint_least32_t hr) noexcept {
  return (hr >> 16) & 0x1FFFu;
}

// NTSTATUS -> equivalent Win32 GetLastError() code; 0 means none. The rule
// always applies: a 0 result definitively means "no equivalent".
inline ::std::uint_least32_t
nt_to_win32_code(::std::uint_least32_t ntstatus) noexcept {
  switch (ntstatus) {
#include "nt_win32_map.hpp"
  }
  return static_cast<::std::uint_least32_t>(0);
}

// Win32 GetLastError() code -> representative NTSTATUS (error severity
// preferred); 0 means none.
inline ::std::uint_least32_t
win32_to_nt_code(::std::uint_least32_t win32err) noexcept {
  switch (win32err) {
#include "win32_nt_map.hpp"
  }
  return static_cast<::std::uint_least32_t>(0);
}

// nt <-> win32 equivalence via the code maps:
//   1 equivalent / 0 definitely not equivalent / -1 unreachable here
// (the rule always applies, so it never reports "no opinion").
inline ::std::int_least8_t
nt_win32_equivalent(::std::uint_least32_t ntstatus,
                    ::std::uint_least32_t win32err) noexcept {
  auto const mapped{nt_to_win32_code(ntstatus)};
  if (mapped == 0) {
    return 0;
  }
  return mapped == win32err ? 1 : 0;
}

// nt <-> com: only an HRESULT carrying FACILITY_NT_BIT equates to the
// embedded NTSTATUS exactly; otherwise there is no specialized rule (-1).
inline constexpr ::std::int_least8_t
nt_com_equivalent(::std::uint_least32_t ntstatus,
                  ::std::uint_least32_t hr) noexcept {
  if ((hr & com_facility_nt_bit) != 0) {
    return (hr & ~com_facility_nt_bit) == ntstatus ? 1 : 0;
  }
  return -1;
}

// com <-> win32: only a FACILITY_WIN32 HRESULT without the NT bit equates
// to its embedded Win32 code; otherwise no specialized rule (-1).
inline constexpr ::std::int_least8_t
com_win32_equivalent(::std::uint_least32_t hr,
                     ::std::uint_least32_t win32err) noexcept {
  if ((hr & com_facility_nt_bit) == 0 &&
      com_hresult_facility(hr) == com_facility_win32) {
    return (hr & com_hresult_code_mask) == win32err ? 1 : 0;
  }
  return -1;
}

#include "nt_message_max.hpp"

// US-English UTF-8 message for an NTSTATUS; len == 0 when absent. Hosted
// builds only: never reference this from freestanding kernel translation
// units or the strings will be embedded into them.
inline ::std::io_scatter_t
nt_u8_message(::std::uint_least32_t ntstatus) noexcept {
  switch (ntstatus) {
#include "nt_message_table.hpp"
  }
  return {nullptr, 0};
}

} // namespace __herbceptions_detail
} // namespace std::error_domains
