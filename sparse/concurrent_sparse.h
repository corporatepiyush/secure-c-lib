#ifndef SCL_CONCURRENT_SPARSE_H
#define SCL_CONCURRENT_SPARSE_H

#include "../common/scl_common.h"
#include <stdatomic.h>
#include <stdint.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef int64_t (*scl_concurrent_sparse_op_t)(int64_t a, int64_t b);

typedef struct {
    int64_t **table;
    size_t n;
    size_t k;
    scl_concurrent_sparse_op_t op;
    atomic_flag lock;
} scl_concurrent_sparse_t;

scl_error_t scl_concurrent_sparse_init(scl_concurrent_sparse_t *st, const int64_t *data, size_t n,
                                       scl_concurrent_sparse_op_t op) SCL_WARN_UNUSED;
void        scl_concurrent_sparse_destroy(scl_concurrent_sparse_t *st);
scl_error_t scl_concurrent_sparse_query(const scl_concurrent_sparse_t *st, size_t l, size_t r,
                                        int64_t *out) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
