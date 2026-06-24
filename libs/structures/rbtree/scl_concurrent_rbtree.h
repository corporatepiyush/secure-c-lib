#ifndef SCL_CONCURRENT_RBTREE_H
#define SCL_CONCURRENT_RBTREE_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef enum { SCL_RB_RED, SCL_RB_BLACK } scl_rb_color_t;

typedef struct scl_concurrent_rbtree_node {
    void *data;
    struct scl_concurrent_rbtree_node *left;
    struct scl_concurrent_rbtree_node *right;
    struct scl_concurrent_rbtree_node *parent;
    scl_rb_color_t color;
} scl_concurrent_rbtree_node_t;

typedef struct {
    scl_concurrent_rbtree_node_t *root;
    size_t element_size;
    atomic_size_t count;
    scl_cmp_func_t cmp;
    scl_spinlock_t lock SCL_CACHE_ALIGNED;
} scl_concurrent_rbtree_t;

scl_error_t scl_crbtree_init(scl_allocator_t *alloc, scl_concurrent_rbtree_t *tree, size_t element_size,
                            scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_crbtree_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_rbtree_t *SCL_RESTRICT tree);
scl_error_t scl_crbtree_insert(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_rbtree_t *SCL_RESTRICT tree, const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_crbtree_remove(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_rbtree_t *SCL_RESTRICT tree, const void *SCL_RESTRICT key) SCL_WARN_UNUSED;
SCL_PURE bool        scl_crbtree_contains(scl_concurrent_rbtree_t *SCL_RESTRICT tree, const void *SCL_RESTRICT key);
scl_error_t scl_crbtree_find(scl_concurrent_rbtree_t *SCL_RESTRICT tree, const void *SCL_RESTRICT key, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
SCL_PURE size_t      scl_crbtree_count(const scl_concurrent_rbtree_t *SCL_RESTRICT tree);
SCL_PURE bool        scl_crbtree_empty(const scl_concurrent_rbtree_t *SCL_RESTRICT tree);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
