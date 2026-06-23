#ifndef SCL_CONCURRENT_FENWICK_H
#define SCL_CONCURRENT_FENWICK_H

#include "scl_concurrent_common.h"

typedef struct {
    unsigned char *tree;
    unsigned char *scratch;
    size_t n;
    size_t element_size;
    void (*add)(void *out, const void *a, const void *b);
    void (*sub)(void *out, const void *a, const void *b);
    scl_spinlock_t lock;
} scl_concurrent_fenwick_t;

scl_error_t scl_cfenwick_init(scl_allocator_t *alloc, scl_concurrent_fenwick_t *fw,
                              size_t n, size_t element_size, const void *data,
                              void (*add)(void *out, const void *a, const void *b),
                              void (*sub)(void *out, const void *a, const void *b)) SCL_WARN_UNUSED;
void        scl_cfenwick_destroy(scl_allocator_t *alloc, scl_concurrent_fenwick_t *fw);
scl_error_t scl_cfenwick_update(scl_allocator_t *alloc, scl_concurrent_fenwick_t *fw,
                                size_t idx, const void *delta) SCL_WARN_UNUSED;
scl_error_t scl_cfenwick_prefix(const scl_concurrent_fenwick_t *fw, size_t idx, void *out) SCL_WARN_UNUSED;
scl_error_t scl_cfenwick_range_query(const scl_concurrent_fenwick_t *fw, size_t l, size_t r, void *out) SCL_WARN_UNUSED;

#endif
