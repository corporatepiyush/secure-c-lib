#ifndef SCL_CONCURRENT_FENWICK_H
#define SCL_CONCURRENT_FENWICK_H

#include "../common/scl_common.h"
#include <stdatomic.h>
#include <stdint.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
    int64_t *tree;
    size_t n;
    atomic_flag lock;
} scl_concurrent_fenwick_t;

scl_error_t scl_concurrent_fenwick_init(scl_concurrent_fenwick_t *ft, const int64_t *data, size_t n) SCL_WARN_UNUSED;
void        scl_concurrent_fenwick_destroy(scl_concurrent_fenwick_t *ft);
scl_error_t scl_concurrent_fenwick_update(scl_concurrent_fenwick_t *ft, size_t index, int64_t delta) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_fenwick_prefix(const scl_concurrent_fenwick_t *ft, size_t index, int64_t *out) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_fenwick_range(const scl_concurrent_fenwick_t *ft, size_t l, size_t r,
                                         int64_t *out) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
