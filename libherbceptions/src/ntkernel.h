#pragma once
/*
Shared NT kernel error category table (private).

NTSTATUS -> {win32, posix, message}, embedded verbatim from
ntkernel-table.ipp (Apache-2.0 / Boost-1.0, Niall Douglas). Used by both the
nt and win32 error domains for cross-domain equivalence and messages.
*/
#include <cstdint>

namespace std::error_domains {
namespace __herbceptions_detail {

// One NTSTATUS table row: NTSTATUS code, equivalent Win32 code, equivalent
// POSIX errno (0 = none), and the US-English UTF-8 descriptive string.
struct ntkernel_field {
  int ntstatus;
  int win32;
  int posix;
  char8_t const *message;
};

inline constexpr ntkernel_field ntkernel_table[] = {
#include "ntkernel-table.ipp"
};

inline constexpr ntkernel_field const *find_ntstatus(int ntstatus) noexcept {
  for (const ntkernel_field &f : ntkernel_table)
    if (f.ntstatus == ntstatus)
      return &f;
  return nullptr;
}

} // namespace __herbceptions_detail
} // namespace std::error_domains
