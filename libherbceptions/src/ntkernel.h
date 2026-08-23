#pragma once
/*
Shared NT kernel error category table (private).

NTSTATUS -> {win32, posix, message}, embedded verbatim from
ntkernel-table.ipp (Apache-2.0 / Boost-1.0, Niall Douglas). Used by both the
nt and win32 error domains for cross-domain equivalence and messages.

Both row layouts are always declared so every translation unit sees the
same types regardless of the build mode. In freestanding kernel mode
find_ntstatus consults the codes-only table (ntkernel-table-fs.ipp): the
descriptive strings are a waste of rom size there and only do_to_errc /
equivalence consult the rows. Message reporting must go through
find_ntstatus_with_message, which is only ever called from code inside the
hosted branch of an if constexpr, so the full table is never emitted in a
freestanding translation unit.
*/
#include <cerrno>
#include <cstddef>
#include <cstdint>

namespace std::error_domains {
namespace __herbceptions_detail {

/*
Mirrors __is_freestanding_kernel_mode (libherbceptions.h); kept here so this
header remains independently includable with only standard headers.
*/
inline constexpr bool __ntkernel_freestanding{
#if __STDC_HOSTED__ == 0 || _KERNEL_MODE == 1
    true
#endif
};

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

// Freestanding row: codes only, no message string.
struct nt_kernel_field_freestanding {
  ::std::uint_least32_t ntstatus;
  ::std::uint_least32_t win32;
  ::std::uint_least32_t posix;
};

inline constexpr ntkernel_field ntkernel_table[] = {
#include "ntkernel-table.ipp"
};

inline constexpr nt_kernel_field_freestanding ntkernel_table_freestanding[] = {
#include "ntkernel-table-fs.ipp"
};

struct ntkernel_win32_field {
  ::std::uint_least32_t win32;
  ::std::uint_least32_t posix;
  ::std::uint_least32_t ntstatus;
  char8_t const *message;
  ::std::size_t message_size;
};

// Static assertions that the tables are sorted ascending by NTSTATUS so the
// binary searches below are valid, and that the embedded message sizes match
// their strings.
namespace {
template <typename T> constexpr bool table_is_sorted(T &table) noexcept {
  constexpr std::size_t count{sizeof(table) / sizeof(*table)};
  for (std::size_t i = 1; i != count; ++i)
    if (table[i - 1].ntstatus >= table[i].ntstatus)
      return false;
  return true;
}
constexpr ::std::size_t my_constexpr_strlen(char8_t const *cstr) noexcept {
  auto it{cstr};
  for (; *it; ++it)
    ;
  return static_cast<::std::size_t>(it - cstr);
}
template <typename T>
constexpr bool table_mesage_size_matches(T &table) noexcept {
  for (auto &e : table)
    if (my_constexpr_strlen(e.message) != e.message_size) {
      return false;
    }
  return true;
}
} // namespace
static_assert(table_is_sorted(ntkernel_table), "ntkernel table must be sorted");
static_assert(table_is_sorted(ntkernel_table_freestanding),
              "ntkernel freestanding table must be sorted");
static_assert(table_mesage_size_matches(ntkernel_table),
              "ntkernel table messages must all match their size");

// Binary search over a sorted-by-ntstatus table; returns nullptr when the
// code is absent.
template <typename T>
inline constexpr T const *
__ntkernel_search(T const *table, ::std::size_t count,
                  ::std::uint_least32_t ntstatus) noexcept {
  std::size_t lo = 0;
  std::size_t hi = count;
  while (lo < hi) {
    std::size_t const mid = lo + (static_cast<::std::size_t>(hi - lo) >> 1u);
    if (table[mid].ntstatus < ntstatus)
      lo = mid + 1u;
    else
      hi = mid;
  }
  if (lo < count && table[lo].ntstatus == ntstatus)
    return table + lo;
  return nullptr;
}

// The tables are sorted ascending by NTSTATUS with unique keys, so a binary
// search finds the row in O(log n) instead of a linear scan. In freestanding
// kernel mode only the codes-only table is searched, so the descriptive
// strings are never referenced.
inline constexpr auto
find_ntstatus(::std::uint_least32_t ntstatus) noexcept {
  if constexpr (__ntkernel_freestanding) {
    return __ntkernel_search(ntkernel_table_freestanding,
                             sizeof(ntkernel_table_freestanding) /
                                 sizeof(*ntkernel_table_freestanding),
                             ntstatus);
  } else {
    return __ntkernel_search(ntkernel_table,
                             sizeof(ntkernel_table) /
                                 sizeof(*ntkernel_table),
                             ntstatus);
  }
}

// Always searches the full table and always returns a full row. Only call
// this when the message is actually going to be reported (hosted builds);
// in freestanding translation units its callers live exclusively inside
// discarded if constexpr branches, keeping the strings out of the binary.
inline constexpr ntkernel_field const *
find_ntstatus_with_message(::std::uint_least32_t ntstatus) noexcept {
  return __ntkernel_search(ntkernel_table,
                           sizeof(ntkernel_table) / sizeof(*ntkernel_table),
                           ntstatus);
}

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

/*
Cross-domain equivalence predicates shared verbatim by the nt, win32 and
com domains' do_equivalent implementations, so every direction of a pair
always agrees regardless of which domain answers the query. Each returns:
  1  the codes are equivalent
  0  the codes are definitely not equivalent
 -1  no specialized rule for this combination (caller compares through
     std::errc instead)

All are inline so the linker discards duplicate copies across the
translation units that embed them.
*/

// nt <-> win32: exact match on the table's win32 column; a zero column
// means "no Win32 equivalent" and never matches (not even ERROR_SUCCESS).
// Codes absent from the table have no exact equivalent either. The rule
// always applies: this never returns -1.
inline ::std::int_least8_t
nt_win32_equivalent(::std::uint_least32_t ntstatus,
                    ::std::uint_least32_t win32err) noexcept {
  auto const f{find_ntstatus(ntstatus)};
  if (f == nullptr || f->win32 == 0) {
    return 0;
  }
  return f->win32 == win32err ? 1 : 0;
}

// nt <-> com: only an HRESULT carrying FACILITY_NT_BIT equates to the
// embedded NTSTATUS exactly.
inline constexpr ::std::int_least8_t
nt_com_equivalent(::std::uint_least32_t ntstatus,
                  ::std::uint_least32_t hr) noexcept {
  if ((hr & com_facility_nt_bit) != 0) {
    return (hr & ~com_facility_nt_bit) == ntstatus ? 1 : 0;
  }
  return -1;
}

// com <-> win32: only a FACILITY_WIN32 HRESULT without the NT bit equates
// to its embedded Win32 code.
inline constexpr ::std::int_least8_t
com_win32_equivalent(::std::uint_least32_t hr,
                     ::std::uint_least32_t win32err) noexcept {
  if ((hr & com_facility_nt_bit) == 0 &&
      com_hresult_facility(hr) == com_facility_win32) {
    return (hr & com_hresult_code_mask) == win32err ? 1 : 0;
  }
  return -1;
}

// The longest descriptive string in the table bounds stack buffers used for
// encoding conversion (every conversion emits at most one output code unit
// per input byte).
inline constexpr ::std::size_t max_ntkernel_message_size() noexcept {
  ::std::size_t mx{};
  for (auto const &e : ntkernel_table)
    if (mx < e.message_size)
      mx = e.message_size;
  return mx;
}

} // namespace __herbceptions_detail
} // namespace std::error_domains
