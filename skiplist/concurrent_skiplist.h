#ifndef SCL_CONCURRENT_SKIPLIST_H
#define SCL_CONCURRENT_SKIPLIST_H

#include "../common/scl_common.h"
#include <stdatomic.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#define SCL_CONCURRENT_SKIPLIST_MAX_LEVEL 16

typedef int (*scl_concurrent_cmp_func_t)(const void *a, const void *b);

typedef struct scl_concurrent_skiplist_node {
    void *data;
    struct scl_concurrent_skiplist_node **forward;
    atomic_size_t level;
} scl_concurrent_skiplist_node_t;

typedef struct {
    scl_concurrent_skiplist_node_t *head;
    size_t element_size;
    atomic_size_t count;
    scl_concurrent_cmp_func_t cmp;
    atomic_size_t level;
    atomic_flag lock;
} scl_concurrent_skiplist_t;

scl_error_t scl_concurrent_skiplist_init(scl_concurrent_skiplist_t *sl, size_t element_size,
                                         scl_concurrent_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_concurrent_skiplist_destroy(scl_concurrent_skiplist_t *sl);
scl_error_t scl_concurrent_skiplist_insert(scl_concurrent_skiplist_t *sl, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_skiplist_remove(scl_concurrent_skiplist_t *sl, const void *key) SCL_WARN_UNUSED;
bool        scl_concurrent_skiplist_contains(const scl_concurrent_skiplist_t *sl, const void *key);
scl_error_t scl_concurrent_skiplist_find(const scl_concurrent_skiplist_t *sl, const void *key, void *out) SCL_WARN_UNUSED;
size_t      scl_concurrent_skiplist_count(const scl_concurrent_skiplist_t *sl);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
