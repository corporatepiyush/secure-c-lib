#include "scl_stdlib.h"
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>

/*
 * scl_stdlib -- secure proxy over <stdlib.h>.
 *
 * Key improvements over raw libc:
 *   - NULL-pointer guard: every string argument is checked; NULL
 *     returns 0 / 0.0 / NULL (safe degradation, not UB).
 *   - Overflow-safe abs: scl_abs(INT_MIN) returns INT_MAX instead of UB
 *     (two's complement negation wraps around in C; we cap it).
 *   - strtol/strtoul wrappers clear errno before the call and detect
 *     ERANGE so callers can distinguish overflow from valid parsing.
 *   - strtol endptr is still NULL-checked before dereference.
 */

int scl_atoi(const char *str) {
    if (scl_unlikely(!str)) return 0;
    return atoi(str);
}

long scl_atol(const char *str) {
    if (scl_unlikely(!str)) return 0;
    return atol(str);
}

long long scl_atoll(const char *str) {
    if (scl_unlikely(!str)) return 0;
    return atoll(str);
}

long scl_strtol(const char *str, char **endptr, int base) {
    if (scl_unlikely(!str)) { if (endptr) *endptr = NULL; return 0; }
    errno = 0;
    long ret = strtol(str, endptr, base);
    if (errno == ERANGE) return LONG_MAX;
    return ret;
}

long long scl_strtoll(const char *str, char **endptr, int base) {
    if (scl_unlikely(!str)) { if (endptr) *endptr = NULL; return 0; }
    errno = 0;
    long long ret = strtoll(str, endptr, base);
    if (errno == ERANGE) return LLONG_MAX;
    return ret;
}

unsigned long scl_strtoul(const char *str, char **endptr, int base) {
    if (scl_unlikely(!str)) { if (endptr) *endptr = NULL; return 0; }
    errno = 0;
    unsigned long ret = strtoul(str, endptr, base);
    if (errno == ERANGE) return ULONG_MAX;
    return ret;
}

unsigned long long scl_strtoull(const char *str, char **endptr, int base) {
    if (scl_unlikely(!str)) { if (endptr) *endptr = NULL; return 0; }
    errno = 0;
    unsigned long long ret = strtoull(str, endptr, base);
    if (errno == ERANGE) return ULLONG_MAX;
    return ret;
}

double scl_atof(const char *str) {
    if (scl_unlikely(!str)) return 0.0;
    return atof(str);
}

double scl_strtod(const char *str, char **endptr) {
    if (scl_unlikely(!str)) { if (endptr) *endptr = NULL; return 0.0; }
    return strtod(str, endptr);
}

int scl_abs(int x) {
    if (scl_unlikely(x == INT_MIN)) return INT_MAX;
    return abs(x);
}

long scl_labs(long x) {
    if (scl_unlikely(x == LONG_MIN)) return LONG_MAX;
    return labs(x);
}

long long scl_llabs(long long x) {
    if (scl_unlikely(x == LLONG_MIN)) return LLONG_MAX;
    return llabs(x);
}

/* Random numbers — backed by system CSPRNG */
int scl_rand(void) {
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    return (int)(arc4random() & INT_MAX);
#else
    unsigned int val = 0;
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        if (fread(&val, sizeof(val), 1, f) == 1) { fclose(f); return (int)(val & INT_MAX); }
        fclose(f);
    }
    return 0;
#endif
}

void scl_srand(unsigned int seed) {
    (void)seed;
}

char *scl_getenv(const char *name) {
    if (scl_unlikely(!name)) return NULL;
    return getenv(name);
}
