#ifndef SCL_CONCURRENT_RBTREE_H
#define SCL_CONCURRENT_RBTREE_H

#include "../common/scl_common.h"
#include <stdatomic.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef int (*scl_concurrent_cmp_func_t)(const void *a, const void *b);

typedef enum { SCL_CONCURRENT_RB_RED, SCL_CONCURRENT_RB_BLACK } scl_concurrent_rb_color_t;

typedef struct scl_concurrent_rbtree_node {
    void *data;
    struct scl_concurrent_rbtree_node *left;
    struct scl_concurrent_rbtree_node *right;
    struct scl_concurrent_rbtree_node *parent;
    scl_concurrent_rb_color_t color;
} scl_concurrent_rbtree_node_t;

typedef struct {
    scl_concurrent_rbtree_node_t *root;
    size_t element_size;
    atomic_size_t count;
    scl_concurrent_cmp_func_t cmp;
    atomic_flag lock;
} scl_concurrent_rbtree_t;

scl_error_t scl_concurrent_rbtree_init(scl_concurrent_rbtree_t *tree, size_t element_size,
                                       scl_concurrent_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_concurrent_rbtree_destroy(scl_concurrent_rbtree_t *tree);
scl_error_t scl_concurrent_rbtree_insert(scl_concurrent_rbtree_t *tree, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_rbtree_remove(scl_concurrent_rbtree_t *tree, const void *key) SCL_WARN_UNUSED;
bool        scl_concurrent_rbtree_contains(scl_concurrent_rbtree_t *tree, const void *key);
scl_error_t scl_concurrent_rbtree_find(scl_concurrent_rbtree_t *tree, const void *key, void *out) SCL_WARN_UNUSED;
size_t      scl_concurrent_rbtree_count(const scl_concurrent_rbtree_t *tree);
bool        scl_concurrent_rbtree_empty(const scl_concurrent_rbtree_t *tree);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
