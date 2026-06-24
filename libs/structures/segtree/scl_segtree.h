#ifndef SCL_SEGTREE_H
#define SCL_SEGTREE_H

#include "scl_common.h"

typedef struct {
    unsigned char *data;
    size_t n;
    size_t size;
    size_t element_size;
    void (*combine)(void *SCL_RESTRICT out, const void *SCL_RESTRICT a, const void *SCL_RESTRICT b);
} scl_segtree_t;

scl_error_t scl_segtree_init(scl_allocator_t *alloc, scl_segtree_t *tree,
                              size_t n, size_t element_size, const void *data,
                              void (*combine)(void *SCL_RESTRICT out, const void *SCL_RESTRICT a, const void *SCL_RESTRICT b)) SCL_WARN_UNUSED;
void        scl_segtree_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_segtree_t *SCL_RESTRICT tree);
scl_error_t scl_segtree_update(scl_segtree_t *SCL_RESTRICT tree, size_t idx, const void *SCL_RESTRICT val) SCL_WARN_UNUSED;
scl_error_t scl_segtree_query(const scl_segtree_t *SCL_RESTRICT tree, size_t l, size_t r, void *SCL_RESTRICT out) SCL_WARN_UNUSED;

#endif
