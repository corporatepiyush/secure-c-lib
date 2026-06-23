#ifndef SCL_STRING_H
#define SCL_STRING_H

#include "scl_common.h"

/* ── String length ───────────────────────────────────────────── */
size_t scl_strlen(const char *s);

/* ── Copy / concat ───────────────────────────────────────────── */
char *scl_strcpy(char *restrict dest, const char *restrict src);
char *scl_strncpy(char *restrict dest, const char *restrict src, size_t n);
char *scl_strcat(char *restrict dest, const char *restrict src);
char *scl_strncat(char *restrict dest, const char *restrict src, size_t n);

/* ── Compare ─────────────────────────────────────────────────── */
int   scl_strcmp(const char *s1, const char *s2);
int   scl_strncmp(const char *s1, const char *s2, size_t n);

/* ── Search ──────────────────────────────────────────────────── */
char *scl_strchr(const char *s, int c);
char *scl_strrchr(const char *s, int c);
char *scl_strstr(const char *haystack, const char *needle);
char *scl_strpbrk(const char *s, const char *accept);
size_t scl_strspn(const char *s, const char *accept);
size_t scl_strcspn(const char *s, const char *reject);

/* ── Tokenise (reentrant) ────────────────────────────────────── */
char *scl_strtok_r(char *restrict str, const char *restrict delim, char **restrict saveptr);

/* ── Duplicate (allocator-based) ─────────────────────────────── */
char *scl_strdup(scl_allocator_t *alloc, const char *src);
char *scl_strndup(scl_allocator_t *alloc, const char *src, size_t maxlen);

/* ── Memory operations ───────────────────────────────────────── */
void *scl_memset(void *s, int c, size_t n);
void *scl_memcpy(void *restrict dest, const void *restrict src, size_t n);
void *scl_memmove(void *dest, const void *src, size_t n);
int   scl_memcmp(const void *s1, const void *s2, size_t n);
void *scl_memchr(const void *s, int c, size_t n);

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
