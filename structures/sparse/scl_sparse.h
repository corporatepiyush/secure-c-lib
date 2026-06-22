#ifndef SCL_SPARSE_H
#define SCL_SPARSE_H

#include "../common/scl_common.h"

typedef struct {
    unsigned char **levels;
    size_t n;
    size_t levels_count;
    size_t element_size;
    unsigned char *scratch;
    void (*combine)(void *out, const void *a, const void *b);
} scl_sparse_t;

scl_error_t scl_sparse_init(scl_allocator_t *alloc, scl_sparse_t *st,
                             size_t n, size_t element_size, const void *data,
                             void (*combine)(void *out, const void *a, const void *b)) SCL_WARN_UNUSED;
void        scl_sparse_destroy(scl_allocator_t *alloc, scl_sparse_t *st);
scl_error_t scl_sparse_query(const scl_sparse_t *st, size_t l, size_t r, void *out) SCL_WARN_UNUSED;

#endif
