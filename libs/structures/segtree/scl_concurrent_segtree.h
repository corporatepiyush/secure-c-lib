#ifndef SCL_CONCURRENT_SEGTREE_H
#define SCL_CONCURRENT_SEGTREE_H

#include "scl_concurrent_common.h"

typedef struct {
    unsigned char *data;
    size_t n;
    size_t size;
    size_t element_size;
    void (*combine)(void *SCL_RESTRICT out, const void *SCL_RESTRICT a, const void *SCL_RESTRICT b);
    scl_spinlock_t lock SCL_CACHE_ALIGNED;
} scl_concurrent_segtree_t;

scl_error_t scl_csegtree_init(scl_allocator_t *alloc, scl_concurrent_segtree_t *tree,
                              size_t n, size_t element_size, const void *data,
                              void (*combine)(void *SCL_RESTRICT out, const void *SCL_RESTRICT a, const void *SCL_RESTRICT b)) SCL_WARN_UNUSED;
void        scl_csegtree_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_segtree_t *SCL_RESTRICT tree);
scl_error_t scl_csegtree_update(scl_allocator_t *alloc, scl_concurrent_segtree_t *tree,
                                size_t idx, const void *val) SCL_WARN_UNUSED;
scl_error_t scl_csegtree_query(const scl_concurrent_segtree_t *SCL_RESTRICT tree, size_t l, size_t r, void *SCL_RESTRICT out) SCL_WARN_UNUSED;

#endif
