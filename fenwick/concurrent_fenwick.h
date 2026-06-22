#ifndef SCL_CONCURRENT_FENWICK_H
#define SCL_CONCURRENT_FENWICK_H

#include "../common/scl_concurrent_common.h"

typedef struct {
    unsigned char *tree;
    unsigned char *scratch;
    size_t n;
    size_t element_size;
    void (*add)(void *out, const void *a, const void *b);
    void (*sub)(void *out, const void *a, const void *b);
    atomic_flag lock;
} scl_atomic_fenwick_t;

scl_error_t scl_atomic_fenwick_init(scl_allocator_t *alloc, scl_atomic_fenwick_t *fw,
                              size_t n, size_t element_size, const void *data,
                              void (*add)(void *out, const void *a, const void *b),
                              void (*sub)(void *out, const void *a, const void *b)) SCL_WARN_UNUSED;
void        scl_atomic_fenwick_destroy(scl_allocator_t *alloc, scl_atomic_fenwick_t *fw);
scl_error_t scl_atomic_fenwick_update(scl_allocator_t *alloc, scl_atomic_fenwick_t *fw,
                                size_t idx, const void *delta) SCL_WARN_UNUSED;
scl_error_t scl_atomic_fenwick_prefix(const scl_atomic_fenwick_t *fw, size_t idx, void *out) SCL_WARN_UNUSED;
scl_error_t scl_atomic_fenwick_range_query(const scl_atomic_fenwick_t *fw, size_t l, size_t r, void *out) SCL_WARN_UNUSED;

#endif
