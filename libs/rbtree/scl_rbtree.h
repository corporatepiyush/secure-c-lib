#ifndef SCL_RBTREE_H
#define SCL_RBTREE_H

#include "scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef enum { SCL_RB_RED, SCL_RB_BLACK } scl_rb_color_t;

typedef struct scl_rbtree_node {
    void *data;
    struct scl_rbtree_node *left;
    struct scl_rbtree_node *right;
    struct scl_rbtree_node *parent;
    scl_rb_color_t color;
} scl_rbtree_node_t;

typedef struct {
    scl_rbtree_node_t *root;
    size_t element_size;
    size_t count;
    scl_cmp_func_t cmp;
} scl_rbtree_t;

scl_error_t scl_rbtree_init(scl_allocator_t *alloc, scl_rbtree_t *tree, size_t element_size, scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_rbtree_destroy(scl_allocator_t *alloc, scl_rbtree_t *tree);
scl_error_t scl_rbtree_insert(scl_allocator_t *alloc, scl_rbtree_t *tree, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_rbtree_remove(scl_allocator_t *alloc, scl_rbtree_t *tree, const void *key) SCL_WARN_UNUSED;
bool        scl_rbtree_contains(const scl_rbtree_t *tree, const void *key);
scl_error_t scl_rbtree_find(const scl_rbtree_t *tree, const void *key, void *out) SCL_WARN_UNUSED;
scl_error_t scl_rbtree_min(const scl_rbtree_t *tree, void *out) SCL_WARN_UNUSED;
scl_error_t scl_rbtree_max(const scl_rbtree_t *tree, void *out) SCL_WARN_UNUSED;
size_t      scl_rbtree_count(const scl_rbtree_t *tree);
bool        scl_rbtree_empty(const scl_rbtree_t *tree);

scl_error_t scl_rbtree_inorder(const scl_rbtree_t *tree, scl_visit_func_t visit, void *ctx) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
