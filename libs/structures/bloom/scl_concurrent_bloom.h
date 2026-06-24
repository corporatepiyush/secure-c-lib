#ifndef SCL_CONCURRENT_BLOOM_H
#define SCL_CONCURRENT_BLOOM_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef size_t (*scl_bloom_hash_t)(const void *data, size_t len, size_t seed);

typedef struct {
    atomic_uchar *bits;
    size_t bit_count;
    size_t byte_count;
    size_t num_hashes;
    scl_bloom_hash_t hash_func;
    atomic_size_t inserted;
} scl_concurrent_bloom_t;

scl_error_t scl_cbloom_init(scl_allocator_t *alloc, scl_concurrent_bloom_t *bf, size_t expected_items,
                           double false_positive_rate,
                           scl_bloom_hash_t hash_func) SCL_WARN_UNUSED;
void        scl_cbloom_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_bloom_t *SCL_RESTRICT bf);
scl_error_t scl_cbloom_insert(scl_concurrent_bloom_t *SCL_RESTRICT bf, const void *SCL_RESTRICT data, size_t len) SCL_WARN_UNUSED;
SCL_PURE bool        scl_cbloom_maybe_contains(const scl_concurrent_bloom_t *SCL_RESTRICT bf, const void *SCL_RESTRICT data, size_t len);
void        scl_cbloom_clear(scl_concurrent_bloom_t *SCL_RESTRICT bf);
SCL_PURE size_t      scl_cbloom_count(const scl_concurrent_bloom_t *SCL_RESTRICT bf);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
