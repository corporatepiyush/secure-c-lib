#ifndef SCL_BTREE_H
#define SCL_BTREE_H

#include "../common/scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#define SCL_BTREE_DEGREE 4

typedef struct scl_btree_node {
    void **keys;
    void **values;
    struct scl_btree_node **children;
    size_t count;
    bool leaf;
} scl_btree_node_t;

typedef struct {
    scl_btree_node_t *root;
    size_t element_size;
    size_t value_size;
    size_t count;
    scl_cmp_func_t cmp;
    int t;
} scl_btree_t;

scl_error_t scl_btree_init(scl_allocator_t *alloc, scl_btree_t *tree, size_t key_size, size_t value_size,
                           int degree, scl_cmp_func_t cmp) SCL_WARN_UNUSED;
void        scl_btree_destroy(scl_allocator_t *alloc, scl_btree_t *tree);
scl_error_t scl_btree_insert(scl_allocator_t *alloc, scl_btree_t *tree, const void *key, const void *value) SCL_WARN_UNUSED;
scl_error_t scl_btree_get(const scl_btree_t *tree, const void *key, void *out_value) SCL_WARN_UNUSED;
scl_error_t scl_btree_remove(scl_allocator_t *alloc, scl_btree_t *tree, const void *key) SCL_WARN_UNUSED;
bool        scl_btree_contains(const scl_btree_t *tree, const void *key);
size_t      scl_btree_count(const scl_btree_t *tree);
bool        scl_btree_empty(const scl_btree_t *tree);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
