#ifndef SCL_FENWICK_H
#define SCL_FENWICK_H

#include "scl_common.h"

typedef struct {
    unsigned char *tree;
    unsigned char *scratch;
    size_t n;
    size_t element_size;
    void (*add)(void *SCL_RESTRICT out, const void *SCL_RESTRICT a, const void *SCL_RESTRICT b);
    void (*sub)(void *SCL_RESTRICT out, const void *SCL_RESTRICT a, const void *SCL_RESTRICT b);
} scl_fenwick_t;

scl_error_t scl_fenwick_init(scl_allocator_t *alloc, scl_fenwick_t *fw,
                              size_t n, size_t element_size, const void *data,
                              void (*add)(void *SCL_RESTRICT out, const void *SCL_RESTRICT a, const void *SCL_RESTRICT b),
                              void (*sub)(void *SCL_RESTRICT out, const void *SCL_RESTRICT a, const void *SCL_RESTRICT b)) SCL_WARN_UNUSED;
void        scl_fenwick_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_fenwick_t *SCL_RESTRICT fw);
scl_error_t scl_fenwick_update(scl_fenwick_t *SCL_RESTRICT fw, size_t idx, const void *SCL_RESTRICT delta) SCL_WARN_UNUSED;
scl_error_t scl_fenwick_prefix(const scl_fenwick_t *SCL_RESTRICT fw, size_t idx, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_fenwick_range_query(const scl_fenwick_t *SCL_RESTRICT fw, size_t l, size_t r, void *SCL_RESTRICT out) SCL_WARN_UNUSED;

#endif
