#ifndef SCL_COMMON_H
#define SCL_COMMON_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>
#include <stdbool.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdalign.h>

/* ── Compiler attributes ────────────────────────────────────── */
#ifdef __GNUC__
#define SCL_WARN_UNUSED    __attribute__((warn_unused_result))
#define SCL_NONNULL(...)   __attribute__((nonnull(__VA_ARGS__)))
#define SCL_UNUSED         __attribute__((unused))
#define SCL_HOT            __attribute__((hot))
#define SCL_COLD           __attribute__((cold))
#define SCL_ALWAYS_INLINE  __attribute__((always_inline))
#define SCL_RESTRICT       __restrict
#define scl_likely(x)      __builtin_expect(!!(x), 1)
#define scl_unlikely(x)    __builtin_expect(!!(x), 0)
#else
#define SCL_WARN_UNUSED
#define SCL_NONNULL(...)
#define SCL_UNUSED
#define SCL_HOT
#define SCL_COLD
#define SCL_ALWAYS_INLINE
#define SCL_RESTRICT
#define scl_likely(x)      (x)
#define scl_unlikely(x)    (x)
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
    SCL_ERR_NULL_PTR,          /* null pointer passed */
    SCL_ERR_OUT_OF_MEMORY,     /* memory allocation failed */
    SCL_ERR_SIZE_OVERFLOW,     /* integer overflow on size/offset */
    SCL_ERR_EMPTY,             /* container is empty */
    SCL_ERR_NOT_FOUND,         /* key/element not found */
    SCL_ERR_FULL,              /* container is full */
    SCL_ERR_INVALID_INDEX,     /* index out of range */
    SCL_ERR_INVALID_ARG,       /* invalid argument value */
    SCL_ERR_DUPLICATE,         /* duplicate key/element */
    SCL_ERR_ALLOC,             /* allocator-specific error */
    SCL_ERR_INVALID_STATE,     /* invalid internal state (e.g. negative cycle) */
    SCL_ERR_NOT_IMPLEMENTED,   /* feature not implemented */
    SCL_ERR_PARSE,             /* parse error */
    SCL_ERR_UNSUPPORTED,       /* unsupported feature */
    SCL_ERR_IO,                /* I/O error */
    SCL_ERR_ALIGNMENT,         /* alignment error */
    SCL_ERR_LOCK,              /* lock acquisition failed */
    SCL_ERR_TIMEOUT,           /* operation timed out */
    SCL_ERR_DEADLOCK           /* mutex deadlock detected (self-deadlock) */
} scl_error_t;

/* ── Error string map ───────────────────────────────────────── */
static inline const char *scl_error_string(scl_error_t err) {
    switch (err) {
    case SCL_OK:               return "success";
    case SCL_ERR_NULL_PTR:     return "null pointer";
    case SCL_ERR_OUT_OF_MEMORY:return "out of memory";
    case SCL_ERR_SIZE_OVERFLOW:return "size overflow";
    case SCL_ERR_EMPTY:        return "container empty";
    case SCL_ERR_NOT_FOUND:    return "not found";
    case SCL_ERR_FULL:         return "container full";
    case SCL_ERR_INVALID_INDEX:return "invalid index";
    case SCL_ERR_INVALID_ARG:  return "invalid argument";
    case SCL_ERR_DUPLICATE:    return "duplicate entry";
    case SCL_ERR_ALLOC:        return "allocator error";
    case SCL_ERR_INVALID_STATE:return "invalid state";
    case SCL_ERR_NOT_IMPLEMENTED: return "not implemented";
    case SCL_ERR_PARSE:        return "parse error";
    case SCL_ERR_UNSUPPORTED:  return "unsupported";
    case SCL_ERR_IO:           return "I/O error";
    case SCL_ERR_ALIGNMENT:    return "alignment error";
    case SCL_ERR_LOCK:         return "lock failed";
    case SCL_ERR_TIMEOUT:      return "timeout";
    case SCL_ERR_DEADLOCK:     return "deadlock";
    default:                   return "unknown error";
    }
}

/* ── Overflow-safe arithmetic ──────────────────────────────── */
static inline bool scl_add_overflow(size_t a, size_t b, size_t *out) {
    if (scl_unlikely(a > SIZE_MAX - b)) return true;
    *out = a + b;
    return false;
}

static inline bool scl_mul_overflow(size_t a, size_t b, size_t *out) {
    if (scl_unlikely(a > 0 && b > SIZE_MAX / a)) return true;
    *out = a * b;
    return false;
}

/* Secure memory zero — prevents compiler from eliding the wipe. */
void scl_secure_zero(void *ptr, size_t len);

/* ── Cache-line constants (x86-64 / ARM64) ──────────────────── */
#if defined(SCL_ARCH_ARM64)
#define SCL_CACHE_LINE_SIZE 128
#else
#define SCL_CACHE_LINE_SIZE 64
#endif

#define SCL_CACHE_ALIGNED __attribute__((aligned(SCL_CACHE_LINE_SIZE)))

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

/* ── Allocator interface ────────────────────────────────────── */
typedef struct scl_allocator {
    void *(*malloc_fn)(void *state, size_t size, size_t alignment);
    void *(*calloc_fn)(void *state, size_t count, size_t size, size_t alignment);
    void *(*realloc_fn)(void *state, void *ptr, size_t old_size, size_t new_size, size_t alignment);
    void  (*free_fn)(void *state, void *ptr);
    void *state;
} scl_allocator_t;

/* Default libc-based allocator */
extern scl_allocator_t *scl_allocator_default(void);

/* Convenience wrappers */
static inline void *scl_alloc(scl_allocator_t *a, size_t sz, size_t al) {
    return a->malloc_fn(a->state, sz, al);
}
static inline void *scl_calloc(scl_allocator_t *a, size_t cnt, size_t sz, size_t al) {
    return a->calloc_fn(a->state, cnt, sz, al);
}
static inline void *scl_realloc(scl_allocator_t *a, void *p, size_t old, size_t new, size_t al) {
    return a->realloc_fn(a->state, p, old, new, al);
}
static inline void scl_free(scl_allocator_t *a, void *p) {
    if (p) a->free_fn(a->state, p);
}

/* ── Comparison / visit callbacks ──────────────────────────── */
#ifndef SCL_CMP_FUNC_T_DEFINED
#define SCL_CMP_FUNC_T_DEFINED
typedef int (*scl_cmp_func_t)(const void *, const void *);
#endif

typedef void (*scl_visit_func_t)(void *data, void *ctx);

/* ── Graph types ────────────────────────────────────────────── */
#ifndef SCL_GRAPH_TYPES_DEFINED
#define SCL_GRAPH_TYPES_DEFINED
typedef struct scl_adj_node {
    size_t to;
    int weight;
    struct scl_adj_node *next;
} scl_adj_node_t;

typedef struct {
    scl_adj_node_t **adj;
    size_t vertex_count;
    size_t edge_count;
} scl_graph_t;

typedef struct {
    size_t from;
    size_t to;
    int weight;
} scl_edge_t;
#endif

/* ── Fixed-size string buffer (stack-friendly) ─────────────── */
#define SCL_PATH_MAX 4096

#define SCL_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define SCL_MIN(a,b)      (((a) < (b)) ? (a) : (b))
#define SCL_MAX(a,b)      (((a) > (b)) ? (a) : (b))
#define SCL_CLAMP(x,lo,hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))

/* ── Safe string ops (from scl_common.c) ───────────────────── */
size_t scl_strlcpy(char *dst, const char *src, size_t dsize);
size_t scl_strlcat(char *dst, const char *src, size_t dsize);

/* ── Bit utilities ─────────────────────────────────────────── */
uint32_t scl_bit_ceil_u32(uint32_t v);
size_t   scl_bit_ceil_sz(size_t v);
uint32_t scl_log2_u32(uint32_t v);
size_t   scl_log2_sz(size_t v);

/* ── CPU PAUSE hint ────────────────────────────────────────── */
static inline void scl_cpu_pause(void) {
#if defined(SCL_ARCH_X86_64)
    __asm__ volatile("pause" ::: "memory");
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
