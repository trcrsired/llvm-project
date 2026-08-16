#pragma once
/*
Shared NT kernel error category table (private).

NTSTATUS -> {win32, posix, message}, embedded verbatim from
ntkernel-table.ipp (Apache-2.0 / Boost-1.0, Niall Douglas). Used by both the
nt and win32 error domains for cross-domain equivalence and messages.
*/
#include <cstddef>
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

// Static assertion that the table is sorted ascending by NTSTATUS so the
// binary search below is valid.
namespace {
constexpr bool ntkernel_table_is_sorted() noexcept {
  for (std::size_t i = 1; i < sizeof(ntkernel_table) / sizeof(ntkernel_table[0]);
       ++i)
    if (ntkernel_table[i - 1].ntstatus >= ntkernel_table[i].ntstatus)
      return false;
  return true;
}
} // namespace
static_assert(ntkernel_table_is_sorted(), "ntkernel table must be sorted");

// The table is sorted ascending by NTSTATUS with unique keys, so a binary
// search finds the row in O(log n) instead of a linear scan.
inline constexpr ntkernel_field const *find_ntstatus(int ntstatus) noexcept {
  std::size_t const count =
      sizeof(ntkernel_table) / sizeof(ntkernel_table[0]);
  std::size_t lo = 0;
  std::size_t hi = count;
  while (lo < hi) {
    std::size_t const mid = lo + (hi - lo) / 2;
    if (ntkernel_table[mid].ntstatus < ntstatus)
      lo = mid + 1;
    else
      hi = mid;
  }
  if (lo < count && ntkernel_table[lo].ntstatus == ntstatus)
    return &ntkernel_table[lo];
  return nullptr;
}

} // namespace __herbceptions_detail
} // namespace std::error_domains
