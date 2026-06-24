#include "scl_common.h"

/* ── Default libc-backed allocator ──────────────────────────── */

static void *def_malloc(void *state, size_t size, size_t alignment) {
    (void)state;
    if (scl_unlikely(size == 0)) return NULL;
    if (scl_likely(alignment <= alignof(max_align_t)))
        return malloc(size);
#if defined(_POSIX_C_SOURCE) || defined(__APPLE__)
    void *p = NULL;
    if (scl_unlikely(posix_memalign(&p, alignment, size) != 0)) return NULL;
    return p;
#else
    return malloc(size);
#endif
}

static void *def_calloc(void *state, size_t count, size_t size, size_t alignment) {
    (void)state;
    if (scl_unlikely(count == 0 || size == 0)) return NULL;
    size_t total;
    if (scl_unlikely(__builtin_mul_overflow(count, size, &total))) return NULL;
    void *p = def_malloc(state, total, alignment);
    if (scl_likely(p)) memset(p, 0, total);
    return p;
}

static void *def_realloc(void *state, void *ptr, size_t old_size, size_t new_size, size_t alignment) {
    (void)state;
    if (scl_unlikely(new_size == 0)) { free(ptr); return NULL; }
    if (scl_unlikely(!ptr)) return def_malloc(state, new_size, alignment);
    if (scl_likely(alignment <= alignof(max_align_t)))
        return realloc(ptr, new_size);
    void *newp = def_malloc(state, new_size, alignment);
    if (scl_unlikely(!newp)) return NULL;
    size_t copy_sz = SCL_MIN(old_size, new_size);
    memcpy(newp, ptr, copy_sz);
    free(ptr);
    return newp;
}

static void def_free(void *state, void *ptr) {
    (void)state;
    free(ptr);
}

static scl_allocator_t default_allocator = {
    .malloc_fn = def_malloc,
    .calloc_fn = def_calloc,
    .realloc_fn = def_realloc,
    .free_fn  = def_free,
    .state    = NULL
};

scl_allocator_t *scl_allocator_default(void) {
    return &default_allocator;
}

/* ── Safe string copy (always null-terminates) ─────────────── */
size_t scl_strlcpy(char *SCL_RESTRICT dst, const char *SCL_RESTRICT src, size_t dsize) {
    size_t n = 0;
    while (scl_likely(n + 1 < dsize && src[n])) { dst[n] = src[n]; n++; }
    if (scl_unlikely(dsize == 0)) return strlen(src);
    dst[n] = '\0';
    while (src[n]) n++;
    return n;
}

size_t scl_strlcat(char *SCL_RESTRICT dst, const char *SCL_RESTRICT src, size_t dsize) {
    size_t di = 0;
    while (di < dsize && dst[di]) di++;
    if (scl_unlikely(di == dsize)) return di + strlen(src);
    size_t si = 0;
    while (scl_likely(di + 1 < dsize && src[si])) { dst[di] = src[si]; di++; si++; }
    dst[di] = '\0';
    while (src[si]) si++;
    return di + si;
}

/* ── Secure memory zero (cannot be elided by compiler) ──────── */
static void *(*volatile scl_secure_memset)(void *, int, size_t) = memset;

void scl_secure_zero(void *ptr, size_t len) {
    if (scl_likely(ptr && len))
        (void)scl_secure_memset(ptr, 0, len);
}

/* ── Bit utilities ─────────────────────────────────────────── */
uint32_t scl_bit_ceil_u32(uint32_t v) {
    if (scl_unlikely(v == 0)) return 1;
    v--;
    v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16;
    return v + 1;
}

size_t scl_bit_ceil_sz(size_t v) {
    if (scl_unlikely(v == 0)) return 1;
    v--;
    v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8;
    v |= v >> 16; v |= v >> 32;
    return v + 1;
}

uint32_t scl_log2_u32(uint32_t v) {
#if defined(__GNUC__)
    return (uint32_t)(31 - __builtin_clz(v | 1));
#else
    uint32_t r = 0;
    while (v > 1) { v >>= 1; r++; }
    return r;
#endif
}

size_t scl_log2_sz(size_t v) {
#if defined(__GNUC__)
    return (size_t)((sizeof(size_t) * 8 - 1) - (size_t)__builtin_clzl((unsigned long)(v | 1)));
#else
    size_t r = 0;
    while (v > 1) { v >>= 1; r++; }
    return r;
#endif
}
