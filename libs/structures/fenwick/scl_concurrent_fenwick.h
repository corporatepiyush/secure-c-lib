#ifndef SCL_CONCURRENT_FENWICK_H
#define SCL_CONCURRENT_FENWICK_H

#include "scl_concurrent_common.h"

typedef struct {
    unsigned char *tree;
    unsigned char *scratch;
    size_t n;
    size_t element_size;
    void (*add)(void *SCL_RESTRICT out, const void *SCL_RESTRICT a, const void *SCL_RESTRICT b);
    void (*sub)(void *SCL_RESTRICT out, const void *SCL_RESTRICT a, const void *SCL_RESTRICT b);
    scl_spinlock_t lock SCL_CACHE_ALIGNED;
} scl_concurrent_fenwick_t;

scl_error_t scl_cfenwick_init(scl_allocator_t *alloc, scl_concurrent_fenwick_t *fw,
                              size_t n, size_t element_size, const void *data,
                              void (*add)(void *SCL_RESTRICT out, const void *SCL_RESTRICT a, const void *SCL_RESTRICT b),
                              void (*sub)(void *SCL_RESTRICT out, const void *SCL_RESTRICT a, const void *SCL_RESTRICT b)) SCL_WARN_UNUSED;
void        scl_cfenwick_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_fenwick_t *SCL_RESTRICT fw);
scl_error_t scl_cfenwick_update(scl_allocator_t *alloc, scl_concurrent_fenwick_t *fw,
                                size_t idx, const void *delta) SCL_WARN_UNUSED;
scl_error_t scl_cfenwick_prefix(const scl_concurrent_fenwick_t *SCL_RESTRICT fw, size_t idx, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_cfenwick_range_query(const scl_concurrent_fenwick_t *SCL_RESTRICT fw, size_t l, size_t r, void *SCL_RESTRICT out) SCL_WARN_UNUSED;

#endif
