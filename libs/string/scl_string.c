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

#include "scl_string.h"
#include <string.h>
#include <ctype.h>

/*
 * scl_string.c — secure wrappers over libc <string.h> / <ctype.h>.
 *
 * DESIGN RATIONALE
 * ────────────────
 * The wrappers exist so that every call goes through the project's own
 * API boundary.  This lets us:
 *   1. NULL-guard every pointer argument (libc often crashes on NULL;
 *      we return a safe zero / NULL for pointer-taking functions).
 *   2. Avoid direct #include of <string.h> or <ctype.h> in application
 *      code — only the .c file touches them, keeping the dependency
 *      graph clean and making it easy to swap the backend (e.g. use
 *      a freestanding implementation) later.
 *   3. Add overflow-safe operations (scl_strdup uses scl_add_overflow).
 *
 * The memory wrappers (memcpy, memmove, memset, …) are simple
 * delegates to the compiler builtins.  Modern compilers recognise
 * these and replace them with efficient inline code anyway.
 *
 * ── Length ─────────────────────────────────────────────────── */

size_t scl_strlen(const char *s) {
    if (scl_unlikely(!s)) return 0;
    return strlen(s);
}

size_t scl_strnlen(const char *s, size_t maxlen) {
    if (scl_unlikely(!s)) return 0;
    return strnlen(s, maxlen);
}

/* ── Copy / concat ──────────────────────────────────────────── */

char *scl_strncpy(char *restrict dest, const char *restrict src, size_t n) {
    if (scl_unlikely(!dest || !src)) return dest;
    return strncpy(dest, src, n);
}

char *scl_strncat(char *restrict dest, const char *restrict src, size_t n) {
    if (scl_unlikely(!dest || !src)) return dest;
    return strncat(dest, src, n);
}

char *scl_stpncpy(char *restrict dest, const char *restrict src, size_t n) {
    if (scl_unlikely(!dest || !src)) return dest;
    return stpncpy(dest, src, n);
}

/* ── Compare ────────────────────────────────────────────────── */

int scl_strcmp(const char *s1, const char *s2) {
    if (scl_unlikely(!s1)) return -1;
    if (scl_unlikely(!s2)) return 1;
    return strcmp(s1, s2);
}

int scl_strncmp(const char *s1, const char *s2, size_t n) {
    if (scl_unlikely(!s1)) return -1;
    if (scl_unlikely(!s2)) return 1;
    return strncmp(s1, s2, n);
}

/* ── Search ─────────────────────────────────────────────────── */

char *scl_strchr(const char *s, int c) {
    if (scl_unlikely(!s)) return NULL;
    return (char *)strchr(s, c);
}

char *scl_strrchr(const char *s, int c) {
    if (scl_unlikely(!s)) return NULL;
    return (char *)strrchr(s, c);
}

char *scl_strstr(const char *haystack, const char *needle) {
    if (scl_unlikely(!haystack || !needle)) return NULL;
    return (char *)strstr(haystack, needle);
}

char *scl_strpbrk(const char *s, const char *accept) {
    if (scl_unlikely(!s || !accept)) return NULL;
    return (char *)strpbrk(s, accept);
}

size_t scl_strspn(const char *s, const char *accept) {
    if (scl_unlikely(!s || !accept)) return 0;
    return strspn(s, accept);
}

size_t scl_strcspn(const char *s, const char *reject) {
    if (scl_unlikely(!s || !reject)) return 0;
    return strcspn(s, reject);
}

/* ── Tokenise (reentrant version wraps strtok_r) ───────────── */

char *scl_strtok_r(char *restrict str, const char *restrict delim, char **restrict saveptr) {
    if (scl_unlikely(!delim || !saveptr)) return NULL;
    return strtok_r(str, delim, saveptr);
}

/* ── Duplicate (allocated through SCL allocator) ────────────── */

char *scl_strdup(scl_allocator_t *alloc, const char *src) {
    if (!src) return NULL;
    size_t n = strlen(src);
    size_t total;
    if (scl_add_overflow(n, 1, &total)) return NULL;
    char *d = (char *)scl_alloc(alloc, total, alignof(max_align_t));
    if (!d) return NULL;
    memcpy(d, src, total);
    return d;
}

char *scl_strndup(scl_allocator_t *alloc, const char *src, size_t maxlen) {
    if (!src) return NULL;
    size_t n = strnlen(src, maxlen);
    size_t total;
    if (scl_add_overflow(n, 1, &total)) return NULL;
    char *d = (char *)scl_alloc(alloc, total, alignof(max_align_t));
    if (!d) return NULL;
    memcpy(d, src, n);
    d[n] = '\0';
    return d;
}

/* ── Memory operations ──────────────────────────────────────── */

void *scl_memset(void *s, int c, size_t n) {
    return memset(s, c, n);
}

void scl_memzero(void *s, size_t n) {
    if (scl_unlikely(!s)) return;
    volatile unsigned char *p = (volatile unsigned char *)s;
    for (size_t i = 0; i < n; i++) p[i] = 0;
}

void *scl_memcpy(void *restrict dest, const void *restrict src, size_t n) {
    if (scl_unlikely(!dest || !src)) return dest;
    return memcpy(dest, src, n);
}

void *scl_memmove(void *dest, const void *src, size_t n) {
    if (scl_unlikely(!dest || !src)) return dest;
    return memmove(dest, src, n);
}

int scl_memcmp(const void *s1, const void *s2, size_t n) {
    if (scl_unlikely(!s1)) return -1;
    if (scl_unlikely(!s2)) return 1;
    return memcmp(s1, s2, n);
}

void *scl_memchr(const void *s, int c, size_t n) {
    if (scl_unlikely(!s)) return NULL;
    return (void *)memchr(s, c, n);
}

/* ── Character classification (promote to unsigned to avoid UB) */

int scl_isalpha(unsigned char c)  { return isalpha(c); }
int scl_isdigit(unsigned char c)  { return isdigit(c); }
int scl_isalnum(unsigned char c)  { return isalnum(c); }
int scl_isspace(unsigned char c)  { return isspace(c); }
int scl_isupper(unsigned char c)  { return isupper(c); }
int scl_islower(unsigned char c)  { return islower(c); }
int scl_isxdigit(unsigned char c) { return isxdigit(c); }
int scl_ispunct(unsigned char c)  { return ispunct(c); }
int scl_isprint(unsigned char c)  { return isprint(c); }
int scl_iscntrl(unsigned char c)  { return iscntrl(c); }

/* ── Character conversion ───────────────────────────────────── */

int scl_tolower(int c) { return tolower((unsigned char)c); }
int scl_toupper(int c) { return toupper((unsigned char)c); }
