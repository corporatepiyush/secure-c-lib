#ifndef SCL_STRING_H
#define SCL_STRING_H

/*
 * scl_string — controlled proxy over <string.h> functions.
 * All scl_* code uses these instead of direct str* calls.
 *
 * Architecture-specific / OS-specific inline asm optimisations
 * are placed here (only in scl_stdlib and scl_string).
 */

#include "../common/scl_common.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Length ─────────────────────────────────────────────────── */
size_t scl_strlen(const char *s);

/* ── Copy / concat ────────────────────────────────────────────── */
char  *scl_strcpy(char *dest, const char *src);
char  *scl_strncpy(char *dest, const char *src, size_t n);
char  *scl_strcat(char *dest, const char *src);
char  *scl_strncat(char *dest, const char *src, size_t n);
char  *scl_strdup(scl_allocator_t *alloc, const char *src);

/* ── Compare ──────────────────────────────────────────────────── */
int    scl_strcmp(const char *s1, const char *s2);
int    scl_strncmp(const char *s1, const char *s2, size_t n);

/* ── Search ────────────────────────────────────────────────────── */
char  *scl_strchr(const char *s, int c);
char  *scl_strrchr(const char *s, int c);
char  *scl_strstr(const char *haystack, const char *needle);
char  *scl_strpbrk(const char *s, const char *accept);
size_t scl_strspn(const char *s, const char *accept);
size_t scl_strcspn(const char *s, const char *reject);

/* ── Token ─────────────────────────────────────────────────────── */
char  *scl_strtok(char *str, const char *delim);

/* ── ASM-optimised string ops ──────────────────────────────────── */
size_t scl_strlen_fast(const char *s) SCL_WARN_UNUSED;

#ifdef __cplusplus
}
#endif

#endif
