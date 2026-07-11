/*===---- string.h - Standard header for null-terminated string -----------===*\
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
\*===----------------------------------------------------------------------===*/

#ifndef __CLANG_STRING_H
#define __CLANG_STRING_H

/* If we're hosted, fall back to the system's string.h, which might have
 * additional definitions.
 */

#if __STDC_HOSTED__ && __has_include_next(<string.h>)

#include_next <string.h>

#else

#include <stddef.h>

#ifndef __cplusplus
extern "C" {
#endif

/*
We offer declarations for these C functions,
leaving the implementations to be handled by users.
*/

void *memcpy(void *, const void *, size_t);      /* freestanding */
void *memmove(void *, const void *, size_t);     /* freestanding */
char *strcpy(char *, const char *);              /* freestanding */
char *strncpy(char *, const char *, size_t);     /* freestanding */
char *strcat(char *, const char *);              /* freestanding */
char *strncat(char *, const char *, size_t);     /* freestanding */
int memcmp(const void *, const void *, size_t);  /* freestanding */
int strcmp(const char *, const char *);          /* freestanding */
int strncmp(const char *, const char *, size_t); /* freestanding */
const void *memchr(const void *, int, size_t);   /* freestanding */
void *memchr(void *, int, size_t);               /* freestanding */
const char *strchr(const char *, int);           /* freestanding */
char *strchr(char *, int);                       /* freestanding */
size_t strcspn(const char *, const char *);      /* freestanding */
const char *strpbrk(const char *, const char *); /* freestanding */
char *strpbrk(char *, const char *);             /* freestanding */
const char *strrchr(const char *, int);          /* freestanding */
char *strrchr(char *, int);                      /* freestanding */
size_t strspn(const char *, const char *);       /* freestanding */
const char *strstr(const char *, const char *);  /* freestanding */
char *strstr(char *, const char *);              /* freestanding */
char *strtok(char *, const char *);              /* freestanding */
void *memset(void *, int, size_t);               /* freestanding */
size_t strlen(const char *);                     /* freestanding */
void *memset_explicit(void *, int, size_t);      /* freestanding */

#ifndef __cplusplus
}
#endif

#endif

#endif
