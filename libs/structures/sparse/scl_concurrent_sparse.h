#ifndef SCL_CONCURRENT_SPARSE_H
#define SCL_CONCURRENT_SPARSE_H

#include "scl_concurrent_common.h"

typedef struct {
    unsigned char **levels;
    size_t n;
    size_t levels_count;
    size_t element_size;
    unsigned char *scratch;
    void (*combine)(void *out, const void *a, const void *b);
} scl_concurrent_sparse_t;

scl_error_t scl_csparse_init(scl_allocator_t *alloc, scl_concurrent_sparse_t *st,
                             size_t n, size_t element_size, const void *data,
                             void (*combine)(void *out, const void *a, const void *b)) SCL_WARN_UNUSED;
void        scl_csparse_destroy(scl_allocator_t *alloc, scl_concurrent_sparse_t *st);
scl_error_t scl_csparse_query(const scl_concurrent_sparse_t *st, size_t l, size_t r, void *out) SCL_WARN_UNUSED;

#endif
