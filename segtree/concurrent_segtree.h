#ifndef SCL_CONCURRENT_SEGTREE_H
#define SCL_CONCURRENT_SEGTREE_H

#include "../common/scl_concurrent_common.h"

typedef struct {
    unsigned char *data;
    size_t n;
    size_t size;
    size_t element_size;
    void (*combine)(void *out, const void *a, const void *b);
    atomic_flag lock;
} scl_atomic_segtree_t;

scl_error_t scl_atomic_segtree_init(scl_allocator_t *alloc, scl_atomic_segtree_t *tree,
                              size_t n, size_t element_size, const void *data,
                              void (*combine)(void *out, const void *a, const void *b)) SCL_WARN_UNUSED;
void        scl_atomic_segtree_destroy(scl_allocator_t *alloc, scl_atomic_segtree_t *tree);
scl_error_t scl_atomic_segtree_update(scl_allocator_t *alloc, scl_atomic_segtree_t *tree,
                                size_t idx, const void *val) SCL_WARN_UNUSED;
scl_error_t scl_atomic_segtree_query(const scl_atomic_segtree_t *tree, size_t l, size_t r, void *out) SCL_WARN_UNUSED;

#endif
