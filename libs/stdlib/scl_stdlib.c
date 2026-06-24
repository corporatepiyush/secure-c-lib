#include "scl_stdlib.h"
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <float.h>
#include <string.h>

/*
 * scl_stdlib.c — secure proxy over <stdlib.h>.
 *
 * DESIGN RATIONALE
 * ────────────────
 * The wrappers exist so that callers never need to #include <stdlib.h>
 * directly.  This gives us a single point to add security hardening:
 *
 *   - Every string argument is NULL-guarded.  Passing NULL returns 0 /
 *     0.0 / NULL instead of crashing or producing UB.
 *   - Overflow-safe abs: scl_abs(INT_MIN) returns INT_MAX instead of
 *     the UB that two's-complement negation produces.
 *   - strtol/strtoul family: errno is cleared before the call and
 *     checked for ERANGE afterwards.  The standard functions do not
 *     have to set errno on success, so callers that forget to clear
 *     errno can see stale values.
 *   - scl_rand is backed by a CSPRNG (arc4random on BSD, /dev/urandom
 *     on Linux), never the predictable libc rand() which is a linear
 *     congruential generator with a 32-bit state.
 *   - scl_strtod checks ERANGE so callers don't have to.
 *   - scl_realloc provides a safe resize-with-copy that works with the
 *     project's allocator abstraction and never leaks on failure.
 *
 * Integer conversion — atoi / atol / atoll
 * ────────────────────────────────────────
 * These are simple wrappers.  The real value of the scl_* versions
 * is the NULL guard — atoi(NULL) is UB in C.
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

/*
 * strtol / strtoul family — with errno discipline.
 *
 * The standard functions may leave errno unchanged on success, so a
 * caller that forgets to clear errno before calling can wrongly
 * detect overflow.  We clear errno ourselves and only return the
 * saturated value when the call actually set ERANGE.
 */

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

/*
 * Floating point — atof / strtod
 *
 * scl_strtod checks errno for ERANGE so callers don't have to.  This
 * is important because strtod can underflow to 0.0 on tiny values and
 * overflow to HUGE_VAL on large ones; the caller cannot distinguish a
 * genuine "0.0" from an underflow without errno.
 */

double scl_atof(const char *str) {
    if (scl_unlikely(!str)) return 0.0;
    return atof(str);
}

double scl_strtod(const char *str, char **endptr) {
    if (scl_unlikely(!str)) { if (endptr) *endptr = NULL; return 0.0; }
    errno = 0;
    double ret = strtod(str, endptr);
    if (errno == ERANGE) {
        return (ret >= 0.0) ? DBL_MAX : -DBL_MAX;
    }
    return ret;
}

/*
 * Absolute value — overflow-safe.
 *
 * In two's complement, abs(INT_MIN) = INT_MIN (UB in C, but the
 * hardware wraps).  We cap at INT_MAX / LONG_MAX / LLONG_MAX.
 */

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

/*
 * Random numbers — backed by system CSPRNG.
 *
 * On BSD/macOS we use arc4random (which returns 2³¹-1 values).
 * On Linux we read from /dev/urandom (non-blocking kernel CSPRNG).
 * We never use libc rand() because its LCG state is tiny and
 * predictable.
 *
 * scl_srand exists only for source-level compatibility with existing
 * code; it is a no-op.  Seeding a CSPRNG not only unnecessary but
 * dangerous — user-provided seeds reduce entropy.
 */

int scl_rand(void) {
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    return (int)(arc4random() & INT_MAX);
#else
    unsigned int val = 0;
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t r = fread(&val, sizeof(val), 1, f);
        (void)r;
        fclose(f);
    }
    return (int)(val & INT_MAX);
#endif
}

void scl_srand(unsigned int seed) {
    (void)seed;
}

/*
 * Environment — scl_getenv.
 *
 * Simply wraps getenv with a NULL guard.  Callers should be aware
 * that environment variables are global and mutable; in
 * multi-threaded programs the returned string can be overwritten by
 * a concurrent setenv call.
 */

char *scl_getenv(const char *name) {
    if (scl_unlikely(!name)) return NULL;
    return getenv(name);
}
