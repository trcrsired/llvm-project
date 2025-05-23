//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <__config>
#if _LIBCPP_HAS_FILESYSTEM
#  include <fstream>
#endif
#include <ostream>

#include "std_stream.h"

_LIBCPP_BEGIN_NAMESPACE_STD
_LIBCPP_BEGIN_EXPLICIT_ABI_ANNOTATIONS

_LIBCPP_EXPORTED_FROM_ABI FILE* __get_ostream_file(ostream& __os) {
  // dynamic_cast requires RTTI, this only affects users whose vendor builds
  // the dylib with RTTI disabled. It does not affect users who build with RTTI
  // disabled but use a dylib where the RTTI is enabled.
  //
  // Returning a nullptr means the stream is not considered a terminal and the
  // special terminal handling is not done. The terminal handling is mainly of
  // importance on Windows.
#if _LIBCPP_HAS_RTTI
  auto* __rdbuf = __os.rdbuf();
#  if _LIBCPP_HAS_FILESYSTEM
  if (auto* __buffer = dynamic_cast<filebuf*>(__rdbuf))
    return __buffer->__file_;
#  endif

  if (auto* __buffer = dynamic_cast<__stdoutbuf<char>*>(__rdbuf))
    return __buffer->__file_;
#endif // _LIBCPP_HAS_RTTI

  return nullptr;
}

_LIBCPP_END_EXPLICIT_ABI_ANNOTATIONS
_LIBCPP_END_NAMESPACE_STD
