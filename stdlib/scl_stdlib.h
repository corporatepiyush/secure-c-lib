#ifndef SCL_STDLIB_H
#define SCL_STDLIB_H

/*
 * scl_stdlib — controlled proxy over standard library functions.
 * All scl_* code MUST use these instead of direct malloc/calloc/realloc/free
 * and other stdlib calls, so the caller retains allocation control.
 *
 * Architecture-specific / OS-specific inline asm optimisations
 * are placed here (only in scl_stdlib and scl_string).
 */

#include "../common/scl_common.h"
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Memory (delegated through allocator) ───────────────────── */
/* Use scl_alloc / scl_calloc / scl_realloc / scl_free from scl_common.h
   which go through the scl_allocator_t interface.  No direct malloc. */

/* ── Environment ────────────────────────────────────────────── */
int       scl_putenv(const char *string);
int       scl_setenv(const char *name, const char *value, int overwrite);
char     *scl_getenv(const char *name);

/* ── System ─────────────────────────────────────────────────── */
void      scl_abort(void) __attribute__((noreturn));
int       scl_atexit(void (*func)(void));

/* ── String conversions ─────────────────────────────────────── */
int       scl_atoi(const char *str);
long      scl_atol(const char *str);
long long scl_atoll(const char *str);
double    scl_atof(const char *str);
long      scl_strtol(const char *str, char **endptr, int base);
unsigned long scl_strtoul(const char *str, char **endptr, int base);
long long scl_strtoll(const char *str, char **endptr, int base);
double    scl_strtod(const char *str, char **endptr);

/* ── Character classification ──────────────────────────────── */
static inline int scl_isdigit(int c) { return (c >= '0' && c <= '9'); }

/* ── Searching / sorting ─────────────────────────────────────── */
void     *scl_bsearch(const void *key, const void *base, size_t nmemb,
                      size_t size, scl_cmp_func_t cmp);

/* ── ASM-optimised memory ops (ifdef-guarded per arch) ──────── */
void *scl_memset(void *s, int c, size_t n) SCL_WARN_UNUSED;
void *scl_memcpy(void *dest, const void *src, size_t n);
void *scl_memmove(void *dest, const void *src, size_t n);
int   scl_memcmp(const void *s1, const void *s2, size_t n);

#ifdef __cplusplus
}
#endif

#endif
