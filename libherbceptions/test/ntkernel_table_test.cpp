//===--- ntkernel_table_test.cpp - ntkernel table size test --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Verifies every row of the ntkernel-table.ipp: the message_size field must
// equal the byte length of the (runtime-decoded, ASCII) message string, i.e.
// exactly what strlen reports. Also verifies the table is sorted ascending by
// NTSTATUS and the messages are non-empty.
//
//===----------------------------------------------------------------------===//

#include "ntkernel.h"

#include <cstdio>
#include <cstring>

namespace detail = std::error_domains::__herbceptions_detail;

int main() {
  std::size_t const count =
      sizeof(detail::ntkernel_table) / sizeof(*detail::ntkernel_table);
  std::size_t failures = 0;

  for (std::size_t i = 0; i != count; ++i) {
    detail::ntkernel_field const &row = detail::ntkernel_table[i];
    std::size_t const actual =
        std::strlen(reinterpret_cast<char const *>(row.message));
    if (row.message_size != actual) {
      std::printf("FAIL row %zu ntstatus=0x%x: message_size=%zu actual=%zu\n",
                  i, static_cast<unsigned>(row.ntstatus), row.message_size,
                  actual);
      ++failures;
    }
    if (row.message[0] == 0) {
      std::printf("FAIL row %zu ntstatus=0x%x: empty message\n", i,
                  static_cast<unsigned>(row.ntstatus));
      ++failures;
    }
    if (i != 0 && detail::ntkernel_table[i - 1].ntstatus >= row.ntstatus) {
      std::printf("FAIL row %zu ntstatus=0x%x: not strictly ascending\n", i,
                  static_cast<unsigned>(row.ntstatus));
      ++failures;
    }
  }

  std::printf("ntkernel table: %zu rows, %zu failures\n", count, failures);
  return failures == 0 ? 0 : 1;
}
