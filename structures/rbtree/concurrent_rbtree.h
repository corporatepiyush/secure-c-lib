#ifndef SCL_CONCURRENT_RBTREE_H
#define SCL_CONCURRENT_RBTREE_H

#include "../common/scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef enum { SCL_RB_RED, SCL_RB_BLACK } scl_rb_color_t;

typedef struct scl_atomic_rbtree_node {
    void *data;
    struct scl_atomic_rbtree_node *left;
    struct scl_atomic_rbtree_node *right;
    struct scl_atomic_rbtree_node *parent;
    scl_rb_color_t color;
} scl_atomic_rbtree_node_t;

typedef struct {
    scl_atomic_rbtree_node_t *root;
    size_t element_size;
    atomic_size_t count;
    scl_cmp_func_t cmp;
    atomic_flag lock;
} scl_atomic_rbtree_t;

scl_error_t scl_atomic_rbtree_init(scl_allocator_t *alloc, scl_atomic_rbtree_t *tree, size_t element_size,
                            scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_atomic_rbtree_destroy(scl_allocator_t *alloc, scl_atomic_rbtree_t *tree);
scl_error_t scl_atomic_rbtree_insert(scl_allocator_t *alloc, scl_atomic_rbtree_t *tree, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_atomic_rbtree_remove(scl_allocator_t *alloc, scl_atomic_rbtree_t *tree, const void *key) SCL_WARN_UNUSED;
bool        scl_atomic_rbtree_contains(scl_atomic_rbtree_t *tree, const void *key);
scl_error_t scl_atomic_rbtree_find(scl_atomic_rbtree_t *tree, const void *key, void *out) SCL_WARN_UNUSED;
size_t      scl_atomic_rbtree_count(const scl_atomic_rbtree_t *tree);
bool        scl_atomic_rbtree_empty(const scl_atomic_rbtree_t *tree);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
