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
    atomic_flag lock;
} scl_concurrent_avl_t;

scl_error_t scl_cavl_init(scl_allocator_t *alloc, scl_concurrent_avl_t *tree, size_t element_size,
                         scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_cavl_destroy(scl_allocator_t *alloc, scl_concurrent_avl_t *tree);
scl_error_t scl_cavl_insert(scl_allocator_t *alloc, scl_concurrent_avl_t *tree, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_cavl_remove(scl_allocator_t *alloc, scl_concurrent_avl_t *tree, const void *key) SCL_WARN_UNUSED;
bool        scl_cavl_contains(scl_concurrent_avl_t *tree, const void *key);
scl_error_t scl_cavl_find(scl_concurrent_avl_t *tree, const void *key, void *out) SCL_WARN_UNUSED;
size_t      scl_cavl_count(const scl_concurrent_avl_t *tree);
bool        scl_cavl_empty(const scl_concurrent_avl_t *tree);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
