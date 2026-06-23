#ifndef SCL_CONCURRENT_HASH_H
#define SCL_CONCURRENT_HASH_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef size_t (*scl_hash_func_t)(const void *key, size_t len);

typedef struct scl_concurrent_hash_entry {
    void *key;
    void *value;
    struct scl_concurrent_hash_entry *next;
} scl_concurrent_hash_entry_t;

typedef struct {
    scl_spinlock_t lock;
} scl_concurrent_hash_bucket_t;

typedef struct {
    scl_concurrent_hash_entry_t **entries;
    scl_concurrent_hash_bucket_t *buckets;
    size_t bucket_count;
    atomic_size_t count;
    scl_hash_func_t hash_func;
    size_t key_size;
    size_t value_size;
} scl_concurrent_hash_t;

scl_error_t scl_chash_init(scl_allocator_t *alloc, scl_concurrent_hash_t *ht, size_t key_size, size_t value_size,
                          size_t bucket_count, scl_hash_func_t hf) SCL_WARN_UNUSED;
void        scl_chash_destroy(scl_allocator_t *alloc, scl_concurrent_hash_t *ht);
scl_error_t scl_chash_insert(scl_allocator_t *alloc, scl_concurrent_hash_t *ht, const void *key, const void *value) SCL_WARN_UNUSED;
scl_error_t scl_chash_get(const scl_concurrent_hash_t *ht, const void *key, void *out_value) SCL_WARN_UNUSED;
scl_error_t scl_chash_remove(scl_allocator_t *alloc, scl_concurrent_hash_t *ht, const void *key) SCL_WARN_UNUSED;
bool        scl_chash_contains(const scl_concurrent_hash_t *ht, const void *key);
size_t      scl_chash_count(const scl_concurrent_hash_t *ht);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
