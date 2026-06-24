#ifndef SCL_CONCURRENT_AVL_H
#define SCL_CONCURRENT_AVL_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_concurrent_avl_node {
    void *data;
    struct scl_concurrent_avl_node *left;
    struct scl_concurrent_avl_node *right;
    int height;
} scl_concurrent_avl_node_t;

typedef struct {
    scl_concurrent_avl_node_t *root;
    size_t element_size;
    atomic_size_t count;
    scl_cmp_func_t cmp;
    scl_spinlock_t lock;
} scl_concurrent_avl_t;

scl_error_t scl_cavl_init(scl_allocator_t *alloc, scl_concurrent_avl_t *tree, size_t element_size,
                         scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_cavl_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_avl_t *SCL_RESTRICT tree);
scl_error_t scl_cavl_insert(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_avl_t *SCL_RESTRICT tree, const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_cavl_remove(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_avl_t *SCL_RESTRICT tree, const void *SCL_RESTRICT key) SCL_WARN_UNUSED;
SCL_PURE bool        scl_cavl_contains(scl_concurrent_avl_t *SCL_RESTRICT tree, const void *SCL_RESTRICT key);
scl_error_t scl_cavl_find(scl_concurrent_avl_t *SCL_RESTRICT tree, const void *SCL_RESTRICT key, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
SCL_PURE size_t      scl_cavl_count(const scl_concurrent_avl_t *SCL_RESTRICT tree);
SCL_PURE bool        scl_cavl_empty(const scl_concurrent_avl_t *SCL_RESTRICT tree);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
