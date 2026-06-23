#include "scl_string.h"
#include <string.h>
#include <ctype.h>

/* ── Length ─────────────────────────────────────────────────── */

size_t scl_strlen(const char *s) {
    if (scl_unlikely(!s)) return 0;
    return strlen(s);
}

/* ── Copy / concat ──────────────────────────────────────────── */

char *scl_strncpy(char *restrict dest, const char *restrict src, size_t n) {
    return strncpy(dest, src, n);
}

char *scl_strncat(char *restrict dest, const char *restrict src, size_t n) {
    return strncat(dest, src, n);
}

/* ── Compare ────────────────────────────────────────────────── */

int scl_strcmp(const char *s1, const char *s2) {
    return strcmp(s1, s2);
}

int scl_strncmp(const char *s1, const char *s2, size_t n) {
    return strncmp(s1, s2, n);
}

/* ── Search ─────────────────────────────────────────────────── */

char *scl_strchr(const char *s, int c)  { return (char *)strchr(s, c); }
char *scl_strrchr(const char *s, int c) { return (char *)strrchr(s, c); }

char *scl_strstr(const char *haystack, const char *needle) {
    return (char *)strstr(haystack, needle);
}

char *scl_strpbrk(const char *s, const char *accept) {
    return (char *)strpbrk(s, accept);
}

size_t scl_strspn(const char *s, const char *accept)  { return strspn(s, accept); }
size_t scl_strcspn(const char *s, const char *reject) { return strcspn(s, reject); }

/* ── Tokenise (reentrant version wraps strtok_r) ───────────── */

char *scl_strtok_r(char *restrict str, const char *restrict delim, char **restrict saveptr) {
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

void *scl_memcpy(void *restrict dest, const void *restrict src, size_t n) {
    return memcpy(dest, src, n);
}

void *scl_memmove(void *dest, const void *src, size_t n) {
    return memmove(dest, src, n);
}

int scl_memcmp(const void *s1, const void *s2, size_t n) {
    return memcmp(s1, s2, n);
}

void *scl_memchr(const void *s, int c, size_t n) {
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
