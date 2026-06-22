#ifndef SCL_CONCURRENT_BST_H
#define SCL_CONCURRENT_BST_H

#include "../common/scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_atomic_bst_node {
    void *data;
    struct scl_atomic_bst_node *left;
    struct scl_atomic_bst_node *right;
} scl_atomic_bst_node_t;

typedef struct {
    scl_atomic_bst_node_t *root;
    size_t element_size;
    atomic_size_t count;
    scl_cmp_func_t cmp;
    atomic_flag lock;
} scl_atomic_bst_t;

scl_error_t scl_atomic_bst_init(scl_allocator_t *alloc, scl_atomic_bst_t *tree, size_t element_size,
                         scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_atomic_bst_destroy(scl_allocator_t *alloc, scl_atomic_bst_t *tree);
scl_error_t scl_atomic_bst_insert(scl_allocator_t *alloc, scl_atomic_bst_t *tree, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_atomic_bst_remove(scl_allocator_t *alloc, scl_atomic_bst_t *tree, const void *key) SCL_WARN_UNUSED;
bool        scl_atomic_bst_contains(scl_atomic_bst_t *tree, const void *key);
scl_error_t scl_atomic_bst_find(scl_atomic_bst_t *tree, const void *key, void *out) SCL_WARN_UNUSED;
size_t      scl_atomic_bst_count(const scl_atomic_bst_t *tree);
bool        scl_atomic_bst_empty(const scl_atomic_bst_t *tree);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
