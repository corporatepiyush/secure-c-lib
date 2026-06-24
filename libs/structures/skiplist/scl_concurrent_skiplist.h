#ifndef SCL_CONCURRENT_SKIPLIST_H
#define SCL_CONCURRENT_SKIPLIST_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#define SCL_SKIPLIST_MAX_LEVEL 16

typedef struct scl_concurrent_skiplist_node {
    void *data;
    struct scl_concurrent_skiplist_node **forward;
    atomic_size_t level;
} scl_concurrent_skiplist_node_t;

typedef struct {
    scl_concurrent_skiplist_node_t *head;
    size_t element_size;
    atomic_size_t count;
    scl_cmp_func_t cmp;
    atomic_size_t level;
    scl_spinlock_t lock SCL_CACHE_ALIGNED;
} scl_concurrent_skiplist_t;

scl_error_t scl_cskiplist_init(scl_allocator_t *alloc, scl_concurrent_skiplist_t *sl, size_t element_size,
                              scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_cskiplist_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_skiplist_t *SCL_RESTRICT sl);
scl_error_t scl_cskiplist_insert(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_skiplist_t *SCL_RESTRICT sl, const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_cskiplist_remove(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_skiplist_t *SCL_RESTRICT sl, const void *SCL_RESTRICT key) SCL_WARN_UNUSED;
SCL_PURE bool        scl_cskiplist_contains(const scl_concurrent_skiplist_t *SCL_RESTRICT sl, const void *SCL_RESTRICT key);
scl_error_t scl_cskiplist_find(const scl_concurrent_skiplist_t *SCL_RESTRICT sl, const void *SCL_RESTRICT key, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
SCL_PURE size_t      scl_cskiplist_count(const scl_concurrent_skiplist_t *SCL_RESTRICT sl);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
