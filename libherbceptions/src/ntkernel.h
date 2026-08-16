#pragma once
/*
Shared NT kernel error category table (private).

NTSTATUS -> {win32, posix, message}, embedded verbatim from
ntkernel-table.ipp (Apache-2.0 / Boost-1.0, Niall Douglas). Used by both the
nt and win32 error domains for cross-domain equivalence and messages.
*/
#include <cerrno>
#include <cstddef>
#include <cstdint>

namespace std::error_domains {
namespace __herbceptions_detail {

// One NTSTATUS table row: NTSTATUS code, equivalent Win32 code, equivalent
// POSIX errno (0 = none), the US-English UTF-8 descriptive string, and its
// byte length (including embedded escapes).
struct ntkernel_field {
  ::std::uint_least32_t ntstatus;
  ::std::uint_least32_t win32;
  ::std::uint_least32_t posix;
  char8_t const *message;
  ::std::size_t message_size;
};

inline constexpr ntkernel_field ntkernel_table[] = {
#include "ntkernel-table.ipp"
};

struct ntkernel_win32_field {
  ::std::uint_least32_t win32;
  ::std::uint_least32_t posix;
  ::std::uint_least32_t ntstatus;
  char8_t const *message;
  ::std::size_t message_size;
};

// Static assertion that the table is sorted ascending by NTSTATUS so the
// binary search below is valid.
namespace {
template<typename T>
constexpr bool table_is_sorted(T &table) noexcept {
  constexpr std::size_t count{sizeof(table) / sizeof(*table)};
  for (std::size_t i = 1; i != count; ++i)
    if (table[i - 1].ntstatus >= table[i].ntstatus)
      return false;
  return true;
}
constexpr ::std::size_t my_constexpr_strlen(char8_t const* cstr) noexcept
{
  auto it{cstr};
  for(;*it;++it);
  return static_cast<::std::size_t>(it-cstr);
}
template<typename T>
constexpr bool table_mesage_size_matches(T &table) noexcept {
  for (auto &e : table)
    if(my_constexpr_strlen(e.message) != e.message_size) {
      return false;
    }
  return true;
}
} // namespace
static_assert(table_is_sorted(ntkernel_table), "ntkernel table must be sorted");
static_assert(table_mesage_size_matches(ntkernel_table), "ntkernel table messages must all match their size");

// The table is sorted ascending by NTSTATUS with unique keys, so a binary
// search finds the row in O(log n) instead of a linear scan.
inline constexpr ntkernel_field const *find_ntstatus(::std::uint_least32_t ntstatus) noexcept {
  constexpr std::size_t count{sizeof(ntkernel_table) / sizeof(*ntkernel_table)};
  std::size_t lo = 0;
  std::size_t hi = count;
  while (lo < hi) {
    std::size_t const mid = lo + (static_cast<::std::size_t>(hi - lo) >> 1u);
    if (ntkernel_table[mid].ntstatus < ntstatus)
      lo = mid + 1u;
    else
      hi = mid;
  }
  if (lo < count && ntkernel_table[lo].ntstatus == ntstatus)
    return ntkernel_table+lo;
  return nullptr;
}

} // namespace __herbceptions_detail
} // namespace std::error_domains
