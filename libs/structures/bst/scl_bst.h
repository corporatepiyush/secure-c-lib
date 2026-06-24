#ifndef SCL_BST_H
#define SCL_BST_H

#include "scl_common.h"

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

scl_error_t scl_bst_init(scl_allocator_t *SCL_RESTRICT alloc, scl_bst_t *SCL_RESTRICT tree, size_t element_size, scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_bst_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_bst_t *SCL_RESTRICT tree);
scl_error_t scl_bst_insert(scl_allocator_t *SCL_RESTRICT alloc, scl_bst_t *SCL_RESTRICT tree, const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_bst_remove(scl_allocator_t *SCL_RESTRICT alloc, scl_bst_t *SCL_RESTRICT tree, const void *SCL_RESTRICT key) SCL_WARN_UNUSED;
SCL_PURE bool        scl_bst_contains(const scl_bst_t *SCL_RESTRICT tree, const void *SCL_RESTRICT key);
scl_error_t scl_bst_find(const scl_bst_t *SCL_RESTRICT tree, const void *SCL_RESTRICT key, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_bst_min(const scl_bst_t *SCL_RESTRICT tree, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_bst_max(const scl_bst_t *SCL_RESTRICT tree, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
SCL_PURE size_t      scl_bst_count(const scl_bst_t *SCL_RESTRICT tree);
SCL_PURE bool        scl_bst_empty(const scl_bst_t *SCL_RESTRICT tree);

scl_error_t scl_bst_inorder(const scl_bst_t *SCL_RESTRICT tree, scl_visit_func_t visit, void *SCL_RESTRICT ctx) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
