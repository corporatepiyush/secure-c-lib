#ifndef SCL_HASH_H
#define SCL_HASH_H

#include "../common/scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef size_t (*scl_hash_func_t)(const void *key, size_t len);
typedef bool   (*scl_hash_eq_func_t)(const void *a, const void *b, size_t key_size);

typedef struct scl_hash_entry {
    void *key;
    size_t key_len;
    void *value;
    size_t value_size;
    struct scl_hash_entry *next;
} scl_hash_entry_t;

typedef struct {
    scl_hash_entry_t **buckets;
    size_t bucket_count;
    size_t count;
    scl_hash_func_t hash_func;
    scl_hash_eq_func_t eq_func;
    size_t key_size;
    size_t value_size;
    float load_factor;
} scl_hash_t;

scl_error_t scl_hash_init(scl_allocator_t *alloc, scl_hash_t *ht, size_t key_size, size_t value_size,
                          size_t initial_buckets, scl_hash_func_t hf,
                          scl_hash_eq_func_t eq) SCL_WARN_UNUSED;
void        scl_hash_destroy(scl_allocator_t *alloc, scl_hash_t *ht);
scl_error_t scl_hash_insert(scl_allocator_t *alloc, scl_hash_t *ht, const void *key, const void *value) SCL_WARN_UNUSED;
scl_error_t scl_hash_get(const scl_hash_t *ht, const void *key, void *out_value) SCL_WARN_UNUSED;
scl_error_t scl_hash_remove(scl_allocator_t *alloc, scl_hash_t *ht, const void *key) SCL_WARN_UNUSED;
bool        scl_hash_contains(const scl_hash_t *ht, const void *key);
size_t      scl_hash_count(const scl_hash_t *ht);

size_t scl_hash_djb2(const void *key, size_t len);
bool   scl_hash_eq_mem(const void *a, const void *b, size_t key_size);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
