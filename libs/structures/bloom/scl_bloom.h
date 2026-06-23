#ifndef SCL_BLOOM_H
#define SCL_BLOOM_H

#include "scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef size_t (*scl_bloom_hash_t)(const void *data, size_t len, size_t seed);

typedef struct {
    unsigned char *bits;
    size_t bit_count;
    size_t byte_count;
    size_t num_hashes;
    scl_bloom_hash_t hash_func;
    size_t inserted;
} scl_bloom_t;

scl_error_t scl_bloom_init(scl_allocator_t *alloc, scl_bloom_t *bf, size_t expected_items, double false_positive_rate,
                           scl_bloom_hash_t hash_func) SCL_WARN_UNUSED;
void        scl_bloom_destroy(scl_allocator_t *alloc, scl_bloom_t *bf);
scl_error_t scl_bloom_insert(scl_bloom_t *bf, const void *data, size_t len) SCL_WARN_UNUSED;
bool        scl_bloom_maybe_contains(const scl_bloom_t *bf, const void *data, size_t len);
void        scl_bloom_clear(scl_bloom_t *bf);
size_t      scl_bloom_count(const scl_bloom_t *bf);
double      scl_bloom_false_positive_rate(const scl_bloom_t *bf);

size_t scl_bloom_hash_murmur(const void *data, size_t len, size_t seed);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
