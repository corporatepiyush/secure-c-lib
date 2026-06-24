#ifndef SCL_SPARSE_H
#define SCL_SPARSE_H

#include "scl_common.h"

typedef struct {
    unsigned char **levels;
    size_t n;
    size_t levels_count;
    size_t element_size;
    unsigned char *scratch;
    void (*combine)(void *SCL_RESTRICT out, const void *SCL_RESTRICT a, const void *SCL_RESTRICT b);
} scl_sparse_t;

scl_error_t scl_sparse_init(scl_allocator_t *alloc, scl_sparse_t *st,
                             size_t n, size_t element_size, const void *data,
                             void (*combine)(void *SCL_RESTRICT out, const void *SCL_RESTRICT a, const void *SCL_RESTRICT b)) SCL_WARN_UNUSED;
void        scl_sparse_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_sparse_t *SCL_RESTRICT st);
scl_error_t scl_sparse_query(const scl_sparse_t *SCL_RESTRICT st, size_t l, size_t r, void *SCL_RESTRICT out) SCL_WARN_UNUSED;

#endif
