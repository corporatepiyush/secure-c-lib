#ifndef SCL_CONCURRENT_BLOOM_H
#define SCL_CONCURRENT_BLOOM_H

#include "../common/scl_common.h"
#include <stdatomic.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef size_t (*scl_concurrent_bloom_hash_t)(const void *data, size_t len, size_t seed);

typedef struct {
    atomic_uchar *bits;
    size_t bit_count;
    size_t byte_count;
    size_t num_hashes;
    scl_concurrent_bloom_hash_t hash_func;
    atomic_size_t inserted;
} scl_concurrent_bloom_t;

scl_error_t scl_concurrent_bloom_init(scl_concurrent_bloom_t *bf, size_t expected_items,
                                      double false_positive_rate,
                                      scl_concurrent_bloom_hash_t hash_func) SCL_WARN_UNUSED;
void        scl_concurrent_bloom_destroy(scl_concurrent_bloom_t *bf);
scl_error_t scl_concurrent_bloom_insert(scl_concurrent_bloom_t *bf, const void *data, size_t len) SCL_WARN_UNUSED;
bool        scl_concurrent_bloom_maybe_contains(const scl_concurrent_bloom_t *bf, const void *data, size_t len);
void        scl_concurrent_bloom_clear(scl_concurrent_bloom_t *bf);
size_t      scl_concurrent_bloom_count(const scl_concurrent_bloom_t *bf);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
