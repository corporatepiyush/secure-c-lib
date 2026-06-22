#include "scl_fenwick.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_fenwick_init(scl_fenwick_t *ft, const int64_t *data, size_t n)
{
    if (!ft || !data) return SCL_ERR_NULL_PTR;
    if (n == 0) return SCL_ERR_INVALID_ARG;

    ft->tree = calloc(n + 1, sizeof(int64_t));
    if (!ft->tree) return SCL_ERR_OUT_OF_MEMORY;

    ft->n = n;

    for (size_t i = 0; i < n; i++) {
        size_t idx = i + 1;
        while (idx <= n) {
            ft->tree[idx] += data[i];
            idx += idx & (size_t)(-(int64_t)idx);
        }
    }
    return SCL_OK;
}

void scl_fenwick_destroy(scl_fenwick_t *ft)
{
    if (ft) {
        free(ft->tree);
        ft->tree = NULL;
        ft->n = 0;
    }
}

scl_error_t scl_fenwick_update(scl_fenwick_t *ft, size_t index, int64_t delta)
{
    if (!ft) return SCL_ERR_NULL_PTR;
    if (index >= ft->n) return SCL_ERR_INVALID_INDEX;

    size_t idx = index + 1;
    while (idx <= ft->n) {
        ft->tree[idx] += delta;
        idx += idx & (size_t)(-(int64_t)idx);
    }
    return SCL_OK;
}

scl_error_t scl_fenwick_prefix(const scl_fenwick_t *ft, size_t index, int64_t *out)
{
    if (!ft || !out) return SCL_ERR_NULL_PTR;
    if (index >= ft->n) return SCL_ERR_INVALID_INDEX;

    int64_t sum = 0;
    size_t idx = index + 1;
    while (idx > 0) {
        sum += ft->tree[idx];
        idx -= idx & (size_t)(-(int64_t)idx);
    }
    *out = sum;
    return SCL_OK;
}

scl_error_t scl_fenwick_range(const scl_fenwick_t *ft, size_t l, size_t r, int64_t *out)
{
    if (!ft || !out) return SCL_ERR_NULL_PTR;
    if (l > r || r >= ft->n) return SCL_ERR_INVALID_INDEX;

    int64_t sum_r = 0, sum_l = 0;
    size_t idx;

    idx = r + 1;
    while (idx > 0) { sum_r += ft->tree[idx]; idx -= idx & (size_t)(-(int64_t)idx); }

    idx = l;
    while (idx > 0) { sum_l += ft->tree[idx]; idx -= idx & (size_t)(-(int64_t)idx); }

    *out = sum_r - sum_l;
    return SCL_OK;
}
