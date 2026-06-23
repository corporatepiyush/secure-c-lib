#ifndef SCL_CONCURRENT_LRU_H
#define SCL_CONCURRENT_LRU_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_concurrent_lru_node {
    void *key;
    void *value;
    struct scl_concurrent_lru_node *prev;
    struct scl_concurrent_lru_node *next;
} scl_concurrent_lru_node_t;

typedef struct {
    scl_concurrent_lru_node_t *head;
    scl_concurrent_lru_node_t *tail;
    size_t capacity;
    atomic_size_t count;
    size_t key_size;
    size_t value_size;
    scl_concurrent_lru_node_t **index;
    size_t index_capacity;
    int (*key_cmp)(const void *, const void *);
    size_t (*key_hash)(const void *, size_t);
    scl_spinlock_t lock;
} scl_concurrent_lru_t;

scl_error_t scl_clru_init(scl_allocator_t *alloc, scl_concurrent_lru_t *cache, size_t key_size, size_t value_size,
                         size_t capacity) SCL_WARN_UNUSED;
void        scl_clru_destroy(scl_allocator_t *alloc, scl_concurrent_lru_t *cache);
scl_error_t scl_clru_put(scl_allocator_t *alloc, scl_concurrent_lru_t *cache, const void *key, const void *value) SCL_WARN_UNUSED;
scl_error_t scl_clru_get(scl_concurrent_lru_t *cache, const void *key, void *out_value) SCL_WARN_UNUSED;
bool        scl_clru_contains(const scl_concurrent_lru_t *cache, const void *key);
scl_error_t scl_clru_remove(scl_allocator_t *alloc, scl_concurrent_lru_t *cache, const void *key) SCL_WARN_UNUSED;
size_t      scl_clru_count(const scl_concurrent_lru_t *cache);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
