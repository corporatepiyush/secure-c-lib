#ifndef SCL_SPARSE_H
#define SCL_SPARSE_H

#include "../common/scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef int64_t (*scl_sparse_op_t)(int64_t a, int64_t b);

typedef struct {
    int64_t **table;
    size_t n;
    size_t k;
    scl_sparse_op_t op;
} scl_sparse_t;

scl_error_t scl_sparse_init(scl_sparse_t *st, const int64_t *data, size_t n,
                            scl_sparse_op_t op) SCL_WARN_UNUSED;
void        scl_sparse_destroy(scl_sparse_t *st);
scl_error_t scl_sparse_query(const scl_sparse_t *st, size_t l, size_t r,
                             int64_t *out) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
