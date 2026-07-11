/*===---- math.h - Standard header for C math ----------------------------===*\
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
\*===----------------------------------------------------------------------===*/

#ifndef __CLANG_MATH_H
#define __CLANG_MATH_H

/* If we're hosted, fall back to the system's math.h, which might have
 * additional definitions.
 */
#if __STDC_HOSTED__ && __has_include_next(<math.h>)

#include_next <math.h>

#else

#define NAN __builtin_nanf("")
#define INFINITY __builtin_inff()
#define FP_NAN 0
#define FP_INFINITE 1
#define FP_ZERO 2
#define FP_SUBNORMAL 3
#define FP_NORMAL 4

#endif
