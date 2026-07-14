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

/* Core type system: error codes (scl_error_t), allocator interface
 * (scl_allocator_t), overflow-safe arithmetic, cache-line alignment macros,
 * branch prediction hints, bounds-checking helpers. Included by every module.
 */

#ifndef SCL_COMMON_H
#define SCL_COMMON_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Lightweight type definitions extracted into dedicated headers ── */
#include "scl_array_types.h"
#include "scl_graph_types.h"
#include "scl_sharded_array_types.h"

/* ── Compiler attributes ────────────────────────────────────── */
#ifdef __GNUC__
#define SCL_WARN_UNUSED __attribute__((warn_unused_result))
#define SCL_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#define SCL_UNUSED __attribute__((unused))
#define SCL_HOT __attribute__((hot))
#define SCL_COLD __attribute__((cold))
#define SCL_ALWAYS_INLINE __attribute__((always_inline))
#define SCL_RESTRICT __restrict
#define scl_likely(x) __builtin_expect(!!(x), 1)
#define scl_unlikely(x) __builtin_expect(!!(x), 0)
#define SCL_CONST __attribute__((const))
#define SCL_PURE __attribute__((pure))
#define SCL_UNREACHABLE __builtin_unreachable()
#define SCL_PRINTF(f, i) __attribute__((format(printf, f, i)))
#else
#define SCL_WARN_UNUSED
#define SCL_NONNULL(...)
#define SCL_UNUSED
#define SCL_HOT
#define SCL_COLD
#define SCL_ALWAYS_INLINE
#define SCL_RESTRICT
#define scl_likely(x) (x)
#define scl_unlikely(x) (x)
#define SCL_CONST
#define SCL_PURE
#define SCL_UNREACHABLE
#define SCL_PRINTF(f, i)
#endif

/* ── OS platform detection ──────────────────────────────────── */
#if defined(__APPLE__)
#define SCL_OS_MACOS 1
#elif defined(__linux__)
#define SCL_OS_LINUX 1
#elif defined(_WIN32) || defined(_WIN64)
#define SCL_OS_WINDOWS 1
#endif

/* ── CPU architecture detection ─────────────────────────────── */
#if defined(__x86_64__) || defined(_M_X64)
#define SCL_ARCH_X86_64 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define SCL_ARCH_ARM64 1
#endif

/* ── Error codes ────────────────────────────────────────────── */
typedef enum {
  SCL_OK = 0,
  SCL_ERR_NULL_PTR,         /* null pointer passed */
  SCL_ERR_OUT_OF_MEMORY,    /* memory allocation failed */
  SCL_ERR_SIZE_OVERFLOW,    /* integer overflow on size/offset */
  SCL_ERR_EMPTY,            /* container is empty */
  SCL_ERR_NOT_FOUND,        /* key/element not found */
  SCL_ERR_FULL,             /* container is full */
  SCL_ERR_INVALID_INDEX,    /* index out of range */
  SCL_ERR_INVALID_ARG,      /* invalid argument value */
  SCL_ERR_DUPLICATE,        /* duplicate key/element */
  SCL_ERR_ALLOC,            /* allocator-specific error */
  SCL_ERR_INVALID_STATE,    /* invalid internal state (e.g. negative cycle) */
  SCL_ERR_NOT_IMPLEMENTED,  /* feature not implemented */
  SCL_ERR_PARSE,            /* parse error */
  SCL_ERR_UNSUPPORTED,      /* unsupported feature */
  SCL_ERR_IO,               /* I/O error */
  SCL_ERR_ALIGNMENT,        /* alignment error */
  SCL_ERR_LOCK,             /* lock acquisition failed */
  SCL_ERR_TIMEOUT,          /* operation timed out */
  SCL_ERR_DEADLOCK,         /* mutex deadlock detected (self-deadlock) */
  SCL_ERR_ML_CONVERGENCE,   /* ML solver failed to converge within max_iter */
  SCL_ERR_ML_SINGULAR,      /* Singular matrix encountered in ML operation */
  SCL_ERR_ML_NO_SOLUTION,   /* No feasible solution (e.g. empty cluster) */
  SCL_ERR_ML_EMPTY_CLUSTER, /* K-Means/GMM centroid with zero assigned points */
  SCL_ERR_ML_MISSING_DATA,  /* NaN or Inf in input data */
  SCL_ERR_ML_OVERFLOW /* Numerical overflow / underflow in ML computation */
} scl_error_t;

/* ── Error string map ───────────────────────────────────────── */
static inline const char *scl_error_string(scl_error_t err) {
  switch (err) {
  case SCL_OK:
    return "success";
  case SCL_ERR_NULL_PTR:
    return "null pointer";
  case SCL_ERR_OUT_OF_MEMORY:
    return "out of memory";
  case SCL_ERR_SIZE_OVERFLOW:
    return "size overflow";
  case SCL_ERR_EMPTY:
    return "container empty";
  case SCL_ERR_NOT_FOUND:
    return "not found";
  case SCL_ERR_FULL:
    return "container full";
  case SCL_ERR_INVALID_INDEX:
    return "invalid index";
  case SCL_ERR_INVALID_ARG:
    return "invalid argument";
  case SCL_ERR_DUPLICATE:
    return "duplicate entry";
  case SCL_ERR_ALLOC:
    return "allocator error";
  case SCL_ERR_INVALID_STATE:
    return "invalid state";
  case SCL_ERR_NOT_IMPLEMENTED:
    return "not implemented";
  case SCL_ERR_PARSE:
    return "parse error";
  case SCL_ERR_UNSUPPORTED:
    return "unsupported";
  case SCL_ERR_IO:
    return "I/O error";
  case SCL_ERR_ALIGNMENT:
    return "alignment error";
  case SCL_ERR_LOCK:
    return "lock failed";
  case SCL_ERR_TIMEOUT:
    return "timeout";
  case SCL_ERR_DEADLOCK:
    return "deadlock";
  case SCL_ERR_ML_CONVERGENCE:
    return "ML: solver did not converge";
  case SCL_ERR_ML_SINGULAR:
    return "ML: singular matrix";
  case SCL_ERR_ML_NO_SOLUTION:
    return "ML: no solution found";
  case SCL_ERR_ML_EMPTY_CLUSTER:
    return "ML: empty cluster";
  case SCL_ERR_ML_MISSING_DATA:
    return "ML: missing data (NaN/Inf)";
  case SCL_ERR_ML_OVERFLOW:
    return "ML: numerical overflow";
  default:
    return "unknown error";
  }
}

/* ── Overflow-safe arithmetic ──────────────────────────────── */
static inline bool scl_add_overflow(size_t a, size_t b, size_t *out) {
  if (scl_unlikely(a > SIZE_MAX - b))
    return true;
  *out = a + b;
  return false;
}

static inline bool scl_mul_overflow(size_t a, size_t b, size_t *out) {
#if defined(__GNUC__) && defined(__SIZEOF_SIZE_T__)
   /* Use the compiler builtin which avoids the division entirely. */
   return __builtin_mul_overflow(a, b, out);
#else
   if (scl_unlikely(a > 0 && b > SIZE_MAX / a))
     return true;
   *out = a * b;
   return false;
#endif
 }

static inline bool scl_sub_overflow(size_t a, size_t b, size_t *out) {
  if (scl_unlikely(b > a))
    return true;
  *out = a - b;
  return false;
}

/* ── Untrusted-input bounds checking ────────────────────────────
 * Generalization of the bounds bugs found across the docparse parsers:
 * any length/offset/count taken from external data must be validated to
 * lie within the real buffer *before* it is used to index or copy. These
 * helpers do the check in an overflow-safe way so call sites can't be
 * fooled by `off + n` wrapping around SIZE_MAX. Prefer these over hand-
 * rolled `p + n <= end` pointer math (which is itself UB on overflow). */

/* True iff the byte range [off, off+n) lies fully within a buffer of
 * `total` bytes. Overflow-safe: a wrapping off+n is rejected, not accepted. */
static inline bool scl_range_in_bounds(size_t total, size_t off, size_t n) {
  if (scl_unlikely(off > total))
    return false;
  return n <= total - off; /* total - off cannot underflow here */
}

/* Largest length, starting at `off`, that still fits in `total` bytes.
 * Use to clamp an attacker-supplied length down to what is actually present. */
static inline size_t scl_clamp_len(size_t total, size_t off, size_t want) {
  if (scl_unlikely(off >= total))
    return 0;
  size_t avail = total - off;
  return want < avail ? want : avail;
}

/* min/max for size_t without the double-evaluation hazard of the macros. */
static inline size_t scl_min_sz(size_t a, size_t b) { return a < b ? a : b; }
static inline size_t scl_max_sz(size_t a, size_t b) { return a > b ? a : b; }

/* Secure memory zero — prevents compiler from eliding the wipe. */
void scl_secure_zero(void *ptr, size_t len);

/* ── Cache-line constants (x86-64 / ARM64) ──────────────────── */
#if defined(SCL_ARCH_ARM64)
#define SCL_CACHE_LINE_SIZE 128
#else
#define SCL_CACHE_LINE_SIZE 64
#endif

#define SCL_CACHE_ALIGNED __attribute__((aligned(SCL_CACHE_LINE_SIZE)))

/* Pad a struct member or trailing field to cache-line boundary.
 * Use SCL_ALIGN_PAD in struct declarations after a write-hot atomic
 * to prevent false sharing on adjacent fields. */
#define SCL_ALIGN_PAD __attribute__((aligned(SCL_CACHE_LINE_SIZE)))

/* Returns padded size rounded up to the nearest cache-line multiple. */
static inline size_t scl_pad_to_cacheline(size_t sz) {
  size_t cl = SCL_CACHE_LINE_SIZE;
  return (sz + cl - 1) & ~(cl - 1);
}

/* ── Memory prefetching ─────────────────────────────────────── */
static inline void scl_prefetch_r(const void *ptr) {
#if defined(SCL_ARCH_X86_64)
  __builtin_prefetch(ptr, 0, 3);
#elif defined(SCL_ARCH_ARM64)
  __builtin_prefetch(ptr, 0, 3);
#else
  (void)ptr;
#endif
}

static inline void scl_prefetch_w(const void *ptr) {
#if defined(SCL_ARCH_X86_64)
  __builtin_prefetch(ptr, 1, 3);
#elif defined(SCL_ARCH_ARM64)
  __builtin_prefetch(ptr, 1, 3);
#else
  (void)ptr;
#endif
}

/* ── Cold-path outlining (use for error handlers) ───────────── */
#ifdef __GNUC__
#define SCL_COLD_PATH __attribute__((cold, noinline))
#else
#define SCL_COLD_PATH
#endif

/* ── Power-of-two checks (zero-allocation hot paths) ────────── */
static inline bool scl_is_pow2_sz(size_t v) {
  return v != 0 && (v & (v - 1)) == 0;
}

static inline bool scl_is_aligned_sz(size_t v, size_t align) {
  return (v & (align - 1)) == 0;
}

/* ── Alignment assumptions (lets compiler use aligned loads) ── */
#ifdef __GNUC__
#define SCL_ASSUME_ALIGNED(p, align) __builtin_assume_aligned((p), (align))
#else
#define SCL_ASSUME_ALIGNED(p, align) (p)
#endif

/* ── Alignment helpers ──────────────────────────────────────── */
static inline size_t scl_align_forward(size_t offset, size_t align) {
  size_t mask = align - 1;
  return (offset + mask) & ~mask;
}

static inline void *scl_ptr_align_forward(void *ptr, size_t align) {
  uintptr_t p = (uintptr_t)ptr;
  size_t mask = align - 1;
  return (void *)((p + mask) & ~mask);
}

/* ── Global allocation cap (opt-in; SIZE_MAX = no limit) ──── */
#ifndef SCL_ALLOC_MAX_SIZE
#define SCL_ALLOC_MAX_SIZE SIZE_MAX
#endif

/* ── Allocator interface ────────────────────────────────────── */
typedef struct scl_allocator {
  void *(*malloc_fn)(void *state, size_t size, size_t alignment);
  void *(*calloc_fn)(void *state, size_t count, size_t size, size_t alignment);
  void *(*realloc_fn)(void *state, void *ptr, size_t old_size, size_t new_size,
                      size_t alignment);
  void (*free_fn)(void *state, void *ptr);
  void *state;
} scl_allocator_t;

/* Default libc-based allocator */
extern scl_allocator_t *scl_allocator_default(void);

/* Convenience wrappers. NULL-tolerant: a NULL allocator falls back to
 * the default (libc) allocator instead of dereferencing NULL — several
 * internal call sites historically passed NULL and crashed on first use.
 * Callers should still pass their allocator so ownership stays explicit. */
static inline void *scl_alloc(scl_allocator_t *a, size_t sz, size_t al) {
  if (scl_unlikely(!a))
    a = scl_allocator_default();
  return a->malloc_fn(a->state, sz, al);
}
static inline void *scl_calloc(scl_allocator_t *a, size_t cnt, size_t sz,
                               size_t al) {
  if (scl_unlikely(!a))
    a = scl_allocator_default();
  return a->calloc_fn(a->state, cnt, sz, al);
}
static inline void *scl_realloc(scl_allocator_t *a, void *p, size_t old,
                                size_t new, size_t al) {
  if (scl_unlikely(!a))
    a = scl_allocator_default();
  return a->realloc_fn(a->state, p, old, new, al);
}
static inline void scl_free(scl_allocator_t *a, void *p) {
  if (p) {
    if (scl_unlikely(!a))
      a = scl_allocator_default();
    a->free_fn(a->state, p);
  }
}

/* ── Comparison / visit callbacks ──────────────────────────── */
#ifndef SCL_CMP_FUNC_T_DEFINED
#define SCL_CMP_FUNC_T_DEFINED
typedef int (*scl_cmp_func_t)(const void *, const void *);
#endif

typedef void (*scl_visit_func_t)(void *data, void *ctx);

/* ── Fixed-size string buffer (stack-friendly) ─────────────── */
#define SCL_PATH_MAX 4096

#define SCL_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define SCL_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define SCL_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define SCL_CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))

/* ── Safe string ops (from scl_common.c) ───────────────────── */
size_t scl_strlcpy(char *dst, const char *src, size_t dsize);
size_t scl_strlcat(char *dst, const char *src, size_t dsize);

/* ── Bit utilities ─────────────────────────────────────────── */
uint32_t scl_bit_ceil_u32(uint32_t v);
size_t scl_bit_ceil_sz(size_t v);
uint32_t scl_log2_u32(uint32_t v);
size_t scl_log2_sz(size_t v);

/* ── CPU PAUSE hint ────────────────────────────────────────── */
static inline void scl_cpu_pause(void) {
#if defined(SCL_ARCH_X86_64)
  __asm__ volatile("pause" ::: "memory");
#elif defined(SCL_ARCH_ARM64) && defined(__APPLE__)
  /* Apple Silicon: 'yield' is a no-op, so use a proper OS yield. */
  sched_yield();
#elif defined(SCL_ARCH_ARM64)
  __asm__ volatile("yield" ::: "memory");
#else
  /* no-op fallback */
#endif
}

/* ── Compiler fence ─────────────────────────────────────────── */
static inline void scl_compiler_barrier(void) {
  __asm__ volatile("" ::: "memory");
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif