/*
 * Copyright 2026 Piyush Katariya
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Safe string/memory ops: bounded scl_strcpy/scl_memset/scl_memcpy, scl_strdup via allocator, scl_secure_zero (volatile-barrier wipe immune to dead-store elimination). */

#ifndef SCL_STRING_H
#define SCL_STRING_H

#include "scl_common.h"

/*
 * scl_string.h — safe replacements for <string.h> and <ctype.h>.
 *
 * Every function that accepts a pointer NULL-guards it: passing NULL
 * either returns 0 / NULL or is explicitly documented as UB (the way
 * the C standard defines it).  The character-classification helpers
 * promote to unsigned char to avoid the undefined behaviour that
 * raw <ctype.h> has for negative values other than EOF.
 *
 * Memory-operation wrappers (memcpy, memmove, memset, …) exist solely
 * so that callers never need to include the libc header directly;
 * they delegate to the builtin/compiler intrinsic where possible.
 *
 * ── String length ───────────────────────────────────────────── */
SCL_PURE size_t scl_strlen(const char * s);
SCL_PURE size_t scl_strnlen(const char * s, size_t maxlen);

/* ── Copy / concat ───────────────────────────────────────────── */
char *scl_strncpy(char * dest, const char * src, size_t n);
char *scl_strncat(char * dest, const char * src, size_t n);
char *scl_stpncpy(char * dest, const char * src, size_t n);

/* ── Compare ─────────────────────────────────────────────────── */
SCL_PURE int   scl_strcmp(const char * s1, const char * s2);
SCL_PURE int   scl_strncmp(const char * s1, const char * s2, size_t n);

/* ── Search ──────────────────────────────────────────────────── */
SCL_PURE char *scl_strchr(const char * s, int c);
SCL_PURE char *scl_strrchr(const char * s, int c);
SCL_PURE char *scl_strstr(const char * haystack, const char * needle);
SCL_PURE char *scl_strpbrk(const char * s, const char * accept);
SCL_PURE size_t scl_strspn(const char * s, const char * accept);
SCL_PURE size_t scl_strcspn(const char * s, const char * reject);

/* ── Tokenise (reentrant) ────────────────────────────────────── */
char *scl_strtok_r(char * str, const char * delim, char **SCL_RESTRICT saveptr);

/* ── Duplicate (allocator-based) ─────────────────────────────── */
char *scl_strdup(scl_allocator_t *alloc, const char *src);
char *scl_strndup(scl_allocator_t *alloc, const char *src, size_t maxlen);

/* ── Memory operations ───────────────────────────────────────── */
void *scl_memset(void * s, int c, size_t n);
void  scl_memzero(void * s, size_t n);
void *scl_memcpy(void * dest, const void * src, size_t n);
void *scl_memmove(void * dest, const void * src, size_t n);
int   scl_memcmp(const void * s1, const void * s2, size_t n);
void *scl_memchr(const void * s, int c, size_t n);

/* ── Character classification ────────────────────────────────── */
int scl_isalpha(unsigned char c);
int scl_isdigit(unsigned char c);
int scl_isalnum(unsigned char c);
int scl_isspace(unsigned char c);
int scl_isupper(unsigned char c);
int scl_islower(unsigned char c);
int scl_isxdigit(unsigned char c);
int scl_ispunct(unsigned char c);
int scl_isprint(unsigned char c);
int scl_iscntrl(unsigned char c);

/* ── Character conversion ────────────────────────────────────── */
int scl_tolower(int c);
int scl_toupper(int c);

#endif /* SCL_STRING_H */
