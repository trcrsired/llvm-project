/*===---- wchar.h - Standard header for wide character types --------------===*\
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
\*===----------------------------------------------------------------------===*/

#ifndef __CLANG_WCHAR_H
#define __CLANG_WCHAR_H

/* If we're hosted, fall back to the system's wchar.h, which might have
 * additional definitions.
 */

#if __STDC_HOSTED__ && __has_include_next(<wchar.h>)

#include_next <wchar.h>

#else

#ifndef __cplusplus
extern "C" {
#endif

/*
We offer declarations for these C functions,
leaving the implementations to be handled by users.
*/

wchar_t *wcscpy(wchar_t *, const wchar_t *);              /* freestanding */
wchar_t *wcsncpy(wchar_t *, const wchar_t *, size_t);     /* freestanding */
wchar_t *wmemcpy(wchar_t *, const wchar_t *, size_t);     /* freestanding */
wchar_t *wmemmove(wchar_t *, const wchar_t *, size_t);    /* freestanding */
wchar_t *wcscat(wchar_t *, const wchar_t *);              /* freestanding */
wchar_t *wcsncat(wchar_t *, const wchar_t *, size_t);     /* freestanding */
int wcscmp(const wchar_t *, const wchar_t *);             /* freestanding */
int wcsncmp(const wchar_t *, const wchar_t *, size_t);    /* freestanding */
int wmemcmp(const wchar_t *, const wchar_t *, size_t);    /* freestanding */
const wchar_t *wcschr(const wchar_t *, wchar_t);          /* freestanding */
wchar_t *wcschr(wchar_t *, wchar_t);                      /* freestanding */
size_t wcscspn(const wchar_t *, const wchar_t *);         /* freestanding */
const wchar_t *wcspbrk(const wchar_t *, const wchar_t *); /* freestanding */
wchar_t *wcspbrk(wchar_t *, const wchar_t *);             /* freestanding */
const wchar_t *wcsrchr(const wchar_t *, wchar_t);         /* freestanding */
wchar_t *wcsrchr(wchar_t *, wchar_t);                     /* freestanding */
size_t wcsspn(const wchar_t *, const wchar_t *);          /* freestanding */
const wchar_t *wcsstr(const wchar_t *, const wchar_t *);  /* freestanding */
wchar_t *wcsstr(wchar_t *, const wchar_t *);              /* freestanding */
wchar_t *wcstok(wchar_t *, const wchar_t *, wchar_t **);  /* freestanding */
const wchar_t *wmemchr(const wchar_t *, wchar_t, size_t); /* freestanding */
wchar_t *wmemchr(wchar_t *, wchar_t, size_t);             /* freestanding */
size_t wcslen(const wchar_t *);                           /* freestanding */
wchar_t *wmemset(wchar_t *, wchar_t, size_t);             /* freestanding */

#ifndef __cplusplus
}
#endif

#endif

#endif
