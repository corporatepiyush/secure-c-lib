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

/* Safe stdlib wrappers: scl_atoi (range-checked), scl_abs (INT_MIN-safe),
 * thread-local splitmix64 PRNG. Bounds-checks all conversions; returns clamped
 * values on overflow. */

#include "scl_stdlib.h"
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/syscall.h>
#endif

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
 * atoi/atol/atoll have *undefined behavior* when the value is not
 * representable (C17 §7.22.1p1), so the scl_* versions are implemented
 * on strtol/strtoll, which are defined to saturate and set ERANGE.
 * Out-of-range input returns the clamped min/max instead of UB.
 */

int scl_atoi(const char *str) {
  if (scl_unlikely(!str))
    return 0;
  errno = 0;
  long v = strtol(str, NULL, 10);
  if (v > INT_MAX)
    return INT_MAX;
  if (v < INT_MIN)
    return INT_MIN;
  return (int)v;
}

long scl_atol(const char *str) {
  if (scl_unlikely(!str))
    return 0;
  errno = 0;
  return strtol(str, NULL, 10); /* saturates to LONG_MIN/LONG_MAX */
}

long long scl_atoll(const char *str) {
  if (scl_unlikely(!str))
    return 0;
  errno = 0;
  return strtoll(str, NULL, 10); /* saturates to LLONG_MIN/LLONG_MAX */
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
  if (scl_unlikely(!str)) {
    if (endptr)
      *endptr = NULL;
    return 0;
  }
  errno = 0;
  /* On ERANGE strtol already returns the correctly-signed saturation
   * (LONG_MIN for negative overflow, LONG_MAX for positive) — pass it
   * through. Forcing LONG_MAX here would turn "-9…9" into a huge
   * positive value. */
  return strtol(str, endptr, base);
}

long long scl_strtoll(const char *str, char **endptr, int base) {
  if (scl_unlikely(!str)) {
    if (endptr)
      *endptr = NULL;
    return 0;
  }
  errno = 0;
  /* Sign-correct saturation: see scl_strtol. */
  return strtoll(str, endptr, base);
}

unsigned long scl_strtoul(const char *str, char **endptr, int base) {
  if (scl_unlikely(!str)) {
    if (endptr)
      *endptr = NULL;
    return 0;
  }
  errno = 0;
  unsigned long ret = strtoul(str, endptr, base);
  if (errno == ERANGE)
    return ULONG_MAX;
  return ret;
}

unsigned long long scl_strtoull(const char *str, char **endptr, int base) {
  if (scl_unlikely(!str)) {
    if (endptr)
      *endptr = NULL;
    return 0;
  }
  errno = 0;
  unsigned long long ret = strtoull(str, endptr, base);
  if (errno == ERANGE)
    return ULLONG_MAX;
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
  if (scl_unlikely(!str))
    return 0.0;
  errno = 0;
  double ret = atof(str);
  /* atof is not required to set errno, but on underflow/overflow
   * it may leave errno unchanged.  Defensive check: if the result
   * is zero but ERANGE is set, it was an underflow. */
  if (ret == 0.0 && errno == ERANGE)
    return 0.0;
  return ret;
}

double scl_strtod(const char *str, char **endptr) {
  if (scl_unlikely(!str)) {
    if (endptr)
      *endptr = NULL;
    return 0.0;
  }
  errno = 0;
  double ret = strtod(str, endptr);
  if (errno == ERANGE) {
    /* ERANGE covers both directions. Overflow returns ±HUGE_VAL —
     * clamp that to ±DBL_MAX. Underflow returns a denormal or 0.0,
     * which must be passed through, not inflated to DBL_MAX. */
    if (ret == HUGE_VAL)
      return DBL_MAX;
    if (ret == -HUGE_VAL)
      return -DBL_MAX;
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
  if (scl_unlikely(x == INT_MIN))
    return INT_MAX;
  return abs(x);
}

long scl_labs(long x) {
  if (scl_unlikely(x == LONG_MIN))
    return LONG_MAX;
  return labs(x);
}

long long scl_llabs(long long x) {
  if (scl_unlikely(x == LLONG_MIN))
    return LLONG_MAX;
  return llabs(x);
}

/*
 * Random numbers — backed by system CSPRNG.
 *
 * On BSD/macOS we use arc4random (which returns 2³¹-1 values).
 * On Linux we use getrandom(2) (a single syscall, no fd, no file open).
 * We never use libc rand() because its LCG state is tiny and
 * predictable.
 *
 * scl_srand exists only for source-level compatibility with existing
 * code; it is a no-op.  Seeding a CSPRNG not only unnecessary but
 * dangerous — user-provided seeds reduce entropy.
 *
 * scl_rand() returns values in [0, INT_MAX] (31 bits).  Callers that
 * need full 32-bit or 64-bit width should use scl_rand_u32() /
 * scl_rand_u64() respectively.
 *
 * Fallback path (getrandom unavailable): we batch-read 256 bytes at a
 * time into a thread-local buffer and serve from it, avoiding a
 * per-call open/read/close cycle.  The fd is opened once with O_CLOEXEC
 * and cached across calls.
 */

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__NetBSD__)
int scl_rand(void) {
  return (int)(arc4random() & INT_MAX);
}

#elif defined(__linux__)

/* Thread-local batch buffer for the fallback /dev/urandom path. */
typedef struct {
  unsigned char buf[256];
  size_t pos;
  size_t len;
} scl_rand_batch_t;

static __thread scl_rand_batch_t scl_rand_batch = {.pos = 0, .len = 0};

/* Refill the thread-local batch buffer from /dev/urandom. */
static bool scl_rand_refill(void) {
  static _Atomic(int) urandom_fd = ATOMIC_VAR_INIT(-1);
  int fd = atomic_load_explicit((_Atomic(int) *)&urandom_fd,
                                memory_order_acquire);
  if (scl_unlikely(fd < 0)) {
    int newfd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (scl_unlikely(newfd < 0))
      return false;
    /* Best-effort: if another thread raced to set it, close our copy. */
    int expected = -1;
    if (!atomic_compare_exchange_weak_explicit(
            (_Atomic(int) *)&urandom_fd, &expected, newfd,
            memory_order_release, memory_order_acquire)) {
      close(newfd);
      fd = expected;
    } else {
      fd = newfd;
    }
  }
  ssize_t r = read(fd, scl_rand_batch.buf, sizeof(scl_rand_batch.buf));
  if (scl_unlikely(r <= 0))
    return false;
  scl_rand_batch.pos = 0;
  scl_rand_batch.len = (size_t)r;
  return true;
}

int scl_rand(void) {
  /* Cache getrandom(2) availability so we don't retry the syscall on
   * every call. 0 = unknown, 1 = works, -1 = failed permanently. */
  static scl_atomic_int getrandom_ok = ATOMIC_VAR_INIT(0);
  int cached =
      scl_atomic_load_int_explicit(&getrandom_ok, scl_memory_order_acquire);
  if (scl_likely(cached > 0)) {
    unsigned int val = 0;
    long ret = syscall(SYS_getrandom, &val, sizeof(val), 0);
    if (scl_unlikely(ret != sizeof(val))) {
      /* Permanent failure — switch to fallback and cache it. */
      scl_atomic_store_int_explicit(&getrandom_ok, -1,
                                    scl_memory_order_release);
      /* fall through to fallback */
    } else {
      return (int)(val & INT_MAX);
    }
  } else if (cached < 0) {
    /* Already failed permanently — go straight to fallback. */
  } else {
    /* First call: try getrandom. */
    unsigned int val = 0;
    long ret = syscall(SYS_getrandom, &val, sizeof(val), 0);
    if (scl_likely(ret == sizeof(val))) {
      scl_atomic_store_int_explicit(&getrandom_ok, 1,
                                    scl_memory_order_release);
      return (int)(val & INT_MAX);
    }
    scl_atomic_store_int_explicit(&getrandom_ok, -1,
                                  scl_memory_order_release);
  }

  /* Fallback: serve from the thread-local batch buffer. */
  if (scl_unlikely(scl_rand_batch.pos >= scl_rand_batch.len)) {
    if (!scl_rand_refill())
      return 0;
  }
  unsigned int val = 0;
  /* Consume up to 4 bytes from the batch buffer. */
  size_t remaining = scl_rand_batch.len - scl_rand_batch.pos;
  if (remaining >= sizeof(val)) {
    memcpy(&val, scl_rand_batch.buf + scl_rand_batch.pos, sizeof(val));
    scl_rand_batch.pos += sizeof(val);
  } else {
    /* Partial read — refill and retry. */
    if (!scl_rand_refill())
      return 0;
    memcpy(&val, scl_rand_batch.buf, sizeof(val));
    scl_rand_batch.pos = sizeof(val);
  }
  return (int)(val & INT_MAX);
}

#else

/* Unknown platform fallback — try getrandom first, then batch-read. */
typedef struct {
  unsigned char buf[256];
  size_t pos;
  size_t len;
} scl_rand_batch_t;

static __thread scl_rand_batch_t scl_rand_batch = {.pos = 0, .len = 0};

static bool scl_rand_refill(void) {
  static _Atomic(int) urandom_fd = ATOMIC_VAR_INIT(-1);
  int fd = atomic_load_explicit((_Atomic(int) *)&urandom_fd,
                                memory_order_acquire);
  if (scl_unlikely(fd < 0)) {
    int newfd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (scl_unlikely(newfd < 0))
      return false;
    int expected = -1;
    if (!atomic_compare_exchange_weak_explicit(
            (_Atomic(int) *)&urandom_fd, &expected, newfd,
            memory_order_release, memory_order_acquire)) {
      close(newfd);
      fd = expected;
    } else {
      fd = newfd;
    }
  }
  ssize_t r = read(fd, scl_rand_batch.buf, sizeof(scl_rand_batch.buf));
  if (scl_unlikely(r <= 0))
    return false;
  scl_rand_batch.pos = 0;
  scl_rand_batch.len = (size_t)r;
  return true;
}

int scl_rand(void) {
  /* Try getrandom(2) first; cache availability so we don't retry
   * the syscall on every call. 0 = unknown, 1 = works, -1 = failed. */
  static scl_atomic_int getrandom_ok = ATOMIC_VAR_INIT(0);
  int cached =
      scl_atomic_load_int_explicit(&getrandom_ok, scl_memory_order_acquire);
  if (scl_likely(cached > 0)) {
    unsigned int val = 0;
    long ret = syscall(SYS_getrandom, &val, sizeof(val), 0);
    if (scl_unlikely(ret != sizeof(val))) {
      scl_atomic_store_int_explicit(&getrandom_ok, -1,
                                    scl_memory_order_release);
    } else {
      return (int)(val & INT_MAX);
    }
  } else if (cached < 0) {
    /* Already failed permanently — go straight to fallback. */
  } else {
    /* First call: try getrandom. */
    unsigned int val = 0;
    long ret = syscall(SYS_getrandom, &val, sizeof(val), 0);
    if (scl_likely(ret == sizeof(val))) {
      scl_atomic_store_int_explicit(&getrandom_ok, 1,
                                    scl_memory_order_release);
      return (int)(val & INT_MAX);
    }
    scl_atomic_store_int_explicit(&getrandom_ok, -1,
                                  scl_memory_order_release);
  }

  /* Fallback: serve from the thread-local batch buffer. */
  if (scl_unlikely(scl_rand_batch.pos >= scl_rand_batch.len)) {
    if (!scl_rand_refill())
      return 0;
  }
  unsigned int val = 0;
  size_t remaining = scl_rand_batch.len - scl_rand_batch.pos;
  if (remaining >= sizeof(val)) {
    memcpy(&val, scl_rand_batch.buf + scl_rand_batch.pos, sizeof(val));
    scl_rand_batch.pos += sizeof(val);
  } else {
    if (!scl_rand_refill())
      return 0;
    memcpy(&val, scl_rand_batch.buf, sizeof(val));
    scl_rand_batch.pos = sizeof(val);
  }
  return (int)(val & INT_MAX);
}

#endif

/* Return a full 32-bit random value (no bias from INT_MAX truncation). */
uint32_t scl_rand_u32(void) {
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) ||      \
    defined(__NetBSD__)
  /* arc4random already yields the full 32 bits — one call, not two. */
  return arc4random();
#else
  uint32_t hi = (uint32_t)scl_rand();
  uint32_t lo = (uint32_t)scl_rand();
  return (hi << 16) ^ lo;
#endif
}

/* Return a full 64-bit random value.
 * On Linux with getrandom, we request 8 bytes directly to avoid
 * the 31-bit truncation of scl_rand().  On other platforms we
 * combine two 32-bit samples. */
uint64_t scl_rand_u64(void) {
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) ||      \
    defined(__NetBSD__)
  /* One arc4random_buf call instead of four arc4random calls. */
  uint64_t val;
  arc4random_buf(&val, sizeof(val));
  return val;
#elif defined(__linux__)
  /* Try getrandom directly for a full 64-bit value. */
  uint64_t val = 0;
  static scl_atomic_int getrandom_ok = ATOMIC_VAR_INIT(0);
  int cached =
      scl_atomic_load_int_explicit(&getrandom_ok, scl_memory_order_acquire);
  if (scl_likely(cached > 0)) {
    long ret = syscall(SYS_getrandom, &val, sizeof(val), 0);
    if (scl_unlikely(ret != sizeof(val))) {
      scl_atomic_store_int_explicit(&getrandom_ok, -1,
                                    scl_memory_order_release);
      /* fall through to safe fallback */
    } else {
      return val;
    }
  } else if (cached >= 0) {
    long ret = syscall(SYS_getrandom, &val, sizeof(val), 0);
    if (scl_likely(ret == sizeof(val))) {
      scl_atomic_store_int_explicit(&getrandom_ok, 1,
                                    scl_memory_order_release);
      return val;
    }
    scl_atomic_store_int_explicit(&getrandom_ok, -1,
                                  scl_memory_order_release);
  }
  /* Fallback: combine two 32-bit values. */
  return ((uint64_t)scl_rand_u32() << 32) | scl_rand_u32();
#else
  return ((uint64_t)scl_rand_u32() << 32) | scl_rand_u32();
#endif
}

void scl_srand(unsigned int seed) { (void)seed; }

/*
 * Environment — scl_getenv.
 *
 * Simply wraps getenv with a NULL guard.  Callers should be aware
 * that environment variables are global and mutable; in
 * multi-threaded programs the returned string can be overwritten by
 * a concurrent setenv call.
 */

char *scl_getenv(const char *name) {
  if (scl_unlikely(!name))
    return NULL;
  return getenv(name);
}