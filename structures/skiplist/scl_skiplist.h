#ifndef SCL_SKIPLIST_H
#define SCL_SKIPLIST_H

#include "../common/scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#define SCL_SKIPLIST_MAX_LEVEL 16

typedef struct scl_skiplist_node {
    void *data;
    struct scl_skiplist_node **forward;
    size_t level;
} scl_skiplist_node_t;

typedef struct {
    scl_skiplist_node_t *head;
    size_t element_size;
    size_t count;
    scl_cmp_func_t cmp;
    size_t level;
} scl_skiplist_t;

scl_error_t scl_skiplist_init(scl_allocator_t *alloc, scl_skiplist_t *sl, size_t element_size,
                              scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_skiplist_destroy(scl_allocator_t *alloc, scl_skiplist_t *sl);
scl_error_t scl_skiplist_insert(scl_allocator_t *alloc, scl_skiplist_t *sl, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_skiplist_remove(scl_allocator_t *alloc, scl_skiplist_t *sl, const void *key) SCL_WARN_UNUSED;
bool        scl_skiplist_contains(const scl_skiplist_t *sl, const void *key);
scl_error_t scl_skiplist_find(const scl_skiplist_t *sl, const void *key, void *out) SCL_WARN_UNUSED;
size_t      scl_skiplist_count(const scl_skiplist_t *sl);
bool        scl_skiplist_empty(const scl_skiplist_t *sl);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
