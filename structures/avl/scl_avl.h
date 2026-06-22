#ifndef SCL_AVL_H
#define SCL_AVL_H

#include "../common/scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_avl_node {
    void *data;
    struct scl_avl_node *left;
    struct scl_avl_node *right;
    int height;
} scl_avl_node_t;

typedef struct {
    scl_avl_node_t *root;
    size_t element_size;
    size_t count;
    scl_cmp_func_t cmp;
} scl_avl_t;

scl_error_t scl_avl_init(scl_allocator_t *alloc, scl_avl_t *tree, size_t element_size, scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_avl_destroy(scl_allocator_t *alloc, scl_avl_t *tree);
scl_error_t scl_avl_insert(scl_allocator_t *alloc, scl_avl_t *tree, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_avl_remove(scl_allocator_t *alloc, scl_avl_t *tree, const void *key) SCL_WARN_UNUSED;
bool        scl_avl_contains(const scl_avl_t *tree, const void *key);
scl_error_t scl_avl_find(const scl_avl_t *tree, const void *key, void *out) SCL_WARN_UNUSED;
scl_error_t scl_avl_min(const scl_avl_t *tree, void *out) SCL_WARN_UNUSED;
scl_error_t scl_avl_max(const scl_avl_t *tree, void *out) SCL_WARN_UNUSED;
size_t      scl_avl_count(const scl_avl_t *tree);
bool        scl_avl_empty(const scl_avl_t *tree);

scl_error_t scl_avl_inorder(const scl_avl_t *tree, scl_visit_func_t visit, void *ctx) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
