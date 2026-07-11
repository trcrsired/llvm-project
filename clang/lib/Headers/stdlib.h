/*===---- stdlib.h - Standard header for general utilities ----------------===*\
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
\*===----------------------------------------------------------------------===*/

#ifndef __CLANG_STDLIB_H
#define __CLANG_STDLIB_H

/* If we're hosted, fall back to the system's math.h, which might have
 * additional definitions.
 */
#if __STDC_HOSTED__ && __has_include_next(<stdlib.h>)

#include_next <stdlib.h>

#else

#include <stddef.h>
#include <stdint.h>

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1 /* freestanding */
#endif
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0 /* freestanding */
#endif

#ifndef __cplusplus
extern "C" {
#endif

typedef struct {
  int quot;
  int rem;
} div_t; /* freestanding */
typedef struct {
  long quot;
  long rem;
} ldiv_t; /* freestanding */
typedef struct {
  long long quot;
  long long rem;
} lldiv_t; /* freestanding */
typedef struct {
  intmax_t quot;
  intmax_t rem;
} imaxdiv_t; /* freestanding */

/*
We offer declarations for these C functions,
leaving the implementations to be handled by users.
*/

// start and termination

#ifndef __CLANG_STDLIB_NOEXCEPT
#ifdef __cplusplus
#if __cplusplus >= 201103L
#define __CLANG_STDLIB_NOEXCEPT noexcept
#else
#define __CLANG_STDLIB_NOEXCEPT throw()
#endif
#else
#define __CLANG_STDLIB_NOEXCEPT
#endif
#endif

[[noreturn]] void abort() __CLANG_STDLIB_NOEXCEPT;         /* freestanding */
int atexit(void (*)()) __CLANG_STDLIB_NOEXCEPT;            /* freestanding */
int at_quick_exit(void (*)()) __CLANG_STDLIB_NOEXCEPT;     /* freestanding */
[[noreturn]] void exit(int);                               /* freestanding */
[[noreturn]] void _Exit(int) __CLANG_STDLIB_NOEXCEPT;      /* freestanding */
[[noreturn]] void quick_exit(int) __CLANG_STDLIB_NOEXCEPT; /* freestanding */

#undef __CLANG_STDLIB_NOEXCEPT

// C standard library algorithms
void *bsearch(const void *, const void *, size_t, size_t,
              int (*)(const void *, const void *)); /* freestanding */
void qsort(void *, size_t, size_t,
           int (*)(const void *, const void *)); /* freestanding */

// absolute values
int abs(int j);                       /* freestanding */
long int abs(long int j);             /* freestanding */
long long int abs(long long int j);   /* freestanding */
long int labs(long int j);            /* freestanding */
long long int llabs(long long int j); /* freestanding */

div_t div(int, int);                         /* freestanding */
ldiv_t div(long int, long int);              /* freestanding */
lldiv_t div(long long int, long long int);   /* freestanding */
ldiv_t ldiv(long int, long int);             /* freestanding */
lldiv_t lldiv(long long int, long long int); /* freestanding */

#ifndef __cplusplus
}
#endif

#endif
