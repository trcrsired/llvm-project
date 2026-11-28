//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once
// Safe-C++ support for libherbceptions.
//
// The implementation names raw pointers, dereferences them and uses legacy
// C++ exception machinery. Under -fsafecxx it is wrapped in a library-header
// [[unsafe]] fence: a syntax relaxation for the implementation that does not
// call-restrict the public API. Legacy-EH entry points are separately marked
// HERBCEPTIONS_UNSAFE_API ([[unsafe]]) because safe source must not use them.
//
// With no safe dialect both macro sets are empty; plain C++ is unchanged.
#if defined(__SAFECXX__)
#define HERBCEPTIONS_SAFECXX_BEGIN [[unsafe]] {
#define HERBCEPTIONS_SAFECXX_END }
#define HERBCEPTIONS_UNSAFE_API [[unsafe]]
// A whole legacy class: every member (including constructors) is call-restricted.
#define HERBCEPTIONS_UNSAFE_CLASS [[unsafe]]
#else
#define HERBCEPTIONS_SAFECXX_BEGIN
#define HERBCEPTIONS_SAFECXX_END
#define HERBCEPTIONS_UNSAFE_API
#define HERBCEPTIONS_UNSAFE_CLASS
#endif
