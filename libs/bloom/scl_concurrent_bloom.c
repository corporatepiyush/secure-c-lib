#include "scl_concurrent_bloom.h"
#include "scl_math.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static size_t default_hash(const void *data, size_t len, size_t seed)
{
    const unsigned char *p = (const unsigned char *)data;
    size_t h = seed;
    for (size_t i = 0; i < len; i++)
        h = h * 0x9e3779b97f4a7c15ULL + p[i];
    return h;
}

scl_error_t scl_cbloom_init(scl_allocator_t *alloc, scl_concurrent_bloom_t *bf, size_t expected_items,
                           double false_positive_rate,
                           scl_bloom_hash_t hash_func)
{
    if (!bf) return SCL_ERR_NULL_PTR;
    if (expected_items == 0 || false_positive_rate <= 0.0 || false_positive_rate >= 1.0)
        return SCL_ERR_INVALID_ARG;
    double ln2 = 0.6931471805599453;
    double lnp = scl_log(false_positive_rate);
    size_t bits = (size_t)(-((double)expected_items * lnp) / (ln2 * ln2));
    if (bits < 64) bits = 64;
    size_t bytes = (bits + 7) / 8;
    size_t num_hashes = (size_t)(((double)bits / (double)expected_items) * ln2);
    if (num_hashes < 1) num_hashes = 1;
    if (num_hashes > 64) num_hashes = 64;
    bf->bits = scl_calloc(alloc, bytes, sizeof(atomic_uchar), alignof(max_align_t));
    if (!bf->bits) return SCL_ERR_OUT_OF_MEMORY;
    bf->bit_count = bits;
    bf->byte_count = bytes;
    bf->num_hashes = num_hashes;
    bf->hash_func = hash_func ? hash_func : default_hash;
    atomic_init(&bf->inserted, 0);
    return SCL_OK;
}

void scl_cbloom_destroy(scl_allocator_t *alloc, scl_concurrent_bloom_t *bf)
{
    if (!bf) return;
    scl_free(alloc, bf->bits);
    bf->bits = NULL;
    bf->bit_count = 0;
    bf->byte_count = 0;
    atomic_store_explicit(&bf->inserted, 0, memory_order_relaxed);
}

scl_error_t scl_cbloom_insert(scl_concurrent_bloom_t *bf, const void *data, size_t len)
{
    if (!bf || !data) return SCL_ERR_NULL_PTR;
    for (size_t i = 0; i < bf->num_hashes; i++) {
        size_t h = bf->hash_func(data, len, i) % bf->bit_count;
        size_t byte_idx = h / 8;
        unsigned char bit_mask = (unsigned char)(1 << (h % 8));
        atomic_fetch_or_explicit(&bf->bits[byte_idx], bit_mask, memory_order_relaxed);
    }
    atomic_fetch_add_explicit(&bf->inserted, 1, memory_order_relaxed);
    return SCL_OK;
}

bool scl_cbloom_maybe_contains(const scl_concurrent_bloom_t *bf, const void *data, size_t len)
{
    if (!bf || !data) return false;
    for (size_t i = 0; i < bf->num_hashes; i++) {
        size_t h = bf->hash_func(data, len, i) % bf->bit_count;
        size_t byte_idx = h / 8;
        unsigned char bit_mask = (unsigned char)(1 << (h % 8));
        unsigned char val = atomic_load_explicit(&bf->bits[byte_idx], memory_order_relaxed);
        if (!(val & bit_mask)) return false;
    }
    return true;
}

void scl_cbloom_clear(scl_concurrent_bloom_t *bf)
{
    if (!bf) return;
    for (size_t i = 0; i < bf->byte_count; i++)
        atomic_store_explicit(&bf->bits[i], 0, memory_order_relaxed);
    atomic_store_explicit(&bf->inserted, 0, memory_order_relaxed);
}

size_t scl_cbloom_count(const scl_concurrent_bloom_t *bf)
{
    return bf ? atomic_load_explicit(&bf->inserted, memory_order_relaxed) : 0;
}
