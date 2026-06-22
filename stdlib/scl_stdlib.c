#include "scl_stdlib.h"
#include <stdlib.h>
#include <string.h>

/* ── Environment ────────────────────────────────────────────── */
int scl_putenv(const char *string) { return putenv((char *)string); }
int scl_setenv(const char *name, const char *value, int overwrite) {
#if defined(SCL_OS_WINDOWS)
    return SetEnvironmentVariableA(name, value) ? 0 : -1;
#else
    return setenv(name, value, overwrite);
#endif
}
char *scl_getenv(const char *name) { return getenv(name); }

/* ── System ─────────────────────────────────────────────────── */
void scl_abort(void) { abort(); }
int scl_atexit(void (*func)(void)) { return atexit(func); }

/* ── String conversions ─────────────────────────────────────── */
int scl_atoi(const char *str) { return atoi(str); }
long scl_atol(const char *str) { return atol(str); }
long long scl_atoll(const char *str) { return atoll(str); }
double scl_atof(const char *str) { return atof(str); }
long scl_strtol(const char *str, char **endptr, int base) { return strtol(str, endptr, base); }
unsigned long scl_strtoul(const char *str, char **endptr, int base) { return strtoul(str, endptr, base); }
long long scl_strtoll(const char *str, char **endptr, int base) { return strtoll(str, endptr, base); }
double scl_strtod(const char *str, char **endptr) { return strtod(str, endptr); }

/* ── bsearch ─────────────────────────────────────────────────── */
void *scl_bsearch(const void *key, const void *base, size_t nmemb,
                  size_t size, scl_cmp_func_t cmp) {
    return bsearch(key, base, nmemb, size, cmp);
}

/* ── ASM-optimised memset (x86_64 ERMSB / ARM64 DC ZVA) ────── */

void *scl_memset(void *s, int c, size_t n) {
    if (n == 0) return s;
#if defined(SCL_ARCH_X86_64) && defined(__GNUC__)
    /* Use REP STOSB on modern x86_64 for large blocks */
    if (n >= 64) {
        __asm__ volatile(
            "cld\n\t"
            "rep stosb\n\t"
            : "+D"(s), "+c"(n)
            : "a"((unsigned char)c)
            : "memory"
        );
        return s;
    }
#elif defined(SCL_ARCH_ARM64) && defined(__GNUC__)
    if (n >= 64) {
        /* DC ZVA zeroing — only for c == 0 */
        if (c == 0) {
            /* ARM64 DC ZVA is complex; fallback to libc for now */
            return memset(s, c, n);
        }
    }
#endif
    return memset(s, c, n);
}

void *scl_memcpy(void *dest, const void *src, size_t n) {
    if (n == 0) return dest;
#if defined(SCL_ARCH_X86_64) && defined(__GNUC__)
    if (n >= 64) {
        __asm__ volatile(
            "cld\n\t"
            "rep movsb\n\t"
            : "+D"(dest), "+S"(src), "+c"(n)
            :
            : "memory"
        );
        return dest;
    }
#endif
    return memcpy(dest, src, n);
}

void *scl_memmove(void *dest, const void *src, size_t n) {
    if (n == 0) return dest;
    return memmove(dest, src, n);
}

int scl_memcmp(const void *s1, const void *s2, size_t n) {
#if defined(SCL_ARCH_X86_64) && defined(__GNUC__)
    int diff = 0;
    __asm__ volatile(
        "cld\n\t"
        "repe cmpsb\n\t"
        "setnz %b0\n\t"
        : "+c"(n), "+D"(s1), "+S"(s2), "=a"(diff)
        :
        : "memory", "cc"
    );
    return diff;
#else
    return memcmp(s1, s2, n);
#endif
}
