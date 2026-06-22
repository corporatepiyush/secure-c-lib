#include "scl_common.h"

/* ── Default libc-backed allocator ──────────────────────────── */
struct scl_default_alloc_state {
    int dummy; /* placeholder */
};

static void *def_malloc(void *state, size_t size, size_t alignment) {
    (void)state;
    (void)alignment;
    return malloc(size);
}

static void *def_calloc(void *state, size_t count, size_t size, size_t alignment) {
    (void)state;
    (void)alignment;
    return calloc(count, size);
}

static void *def_realloc(void *state, void *ptr, size_t old_size, size_t new_size, size_t alignment) {
    (void)state;
    (void)old_size;
    (void)alignment;
    return realloc(ptr, new_size);
}

static void def_free(void *state, void *ptr) {
    (void)state;
    free(ptr);
}

static struct scl_default_alloc_state default_state;
static scl_allocator_t default_allocator = {
    .malloc_fn = def_malloc,
    .calloc_fn = def_calloc,
    .realloc_fn = def_realloc,
    .free_fn  = def_free,
    .state    = &default_state
};

scl_allocator_t *scl_allocator_default(void) {
    return &default_allocator;
}

/* ── Safe string copy (always null-terminates) ─────────────── */
size_t scl_strlcpy(char *dst, const char *src, size_t dsize) {
    size_t n = 0;
    while (n + 1 < dsize && src[n]) { dst[n] = src[n]; n++; }
    if (dsize > 0) dst[n] = '\0';
    while (src[n]) n++;
    return n;
}

size_t scl_strlcat(char *dst, const char *src, size_t dsize) {
    size_t di = 0;
    while (di < dsize && dst[di]) di++;
    size_t si = 0;
    while (di + 1 < dsize && src[si]) { dst[di] = src[si]; di++; si++; }
    if (dsize > 0) dst[di] = '\0';
    while (src[si]) si++;
    return di + si;
}

/* ── Bit utilities ─────────────────────────────────────────── */
uint32_t scl_bit_ceil_u32(uint32_t v) {
    if (v == 0) return 1;
    v--;
    v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16;
    return v + 1;
}

size_t scl_bit_ceil_sz(size_t v) {
    if (v == 0) return 1;
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
