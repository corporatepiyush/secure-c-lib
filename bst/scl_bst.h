#ifndef SCL_BST_H
#define SCL_BST_H

#include "../common/scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_bst_node {
    void *data;
    struct scl_bst_node *left;
    struct scl_bst_node *right;
} scl_bst_node_t;

typedef struct {
    scl_bst_node_t *root;
    size_t element_size;
    size_t count;
    scl_cmp_func_t cmp;
} scl_bst_t;

scl_error_t scl_bst_init(scl_allocator_t *alloc, scl_bst_t *tree, size_t element_size, scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_bst_destroy(scl_allocator_t *alloc, scl_bst_t *tree);
scl_error_t scl_bst_insert(scl_allocator_t *alloc, scl_bst_t *tree, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_bst_remove(scl_allocator_t *alloc, scl_bst_t *tree, const void *key) SCL_WARN_UNUSED;
bool        scl_bst_contains(const scl_bst_t *tree, const void *key);
scl_error_t scl_bst_find(const scl_bst_t *tree, const void *key, void *out) SCL_WARN_UNUSED;
scl_error_t scl_bst_min(const scl_bst_t *tree, void *out) SCL_WARN_UNUSED;
scl_error_t scl_bst_max(const scl_bst_t *tree, void *out) SCL_WARN_UNUSED;
size_t      scl_bst_count(const scl_bst_t *tree);
bool        scl_bst_empty(const scl_bst_t *tree);

scl_error_t scl_bst_inorder(const scl_bst_t *tree, scl_visit_func_t visit, void *ctx) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
