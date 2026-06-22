#include "concurrent_fenwick.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static inline void spin_lock(atomic_flag *lock)
{
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {
        __asm__ volatile("yield");
    }
}

static inline void spin_unlock(atomic_flag *lock)
{
    atomic_flag_clear_explicit(lock, memory_order_release);
}

scl_error_t scl_concurrent_fenwick_init(scl_concurrent_fenwick_t *ft, const int64_t *data, size_t n)
{
    if (!ft) return SCL_ERR_NULL_PTR;
    if (n == 0) return SCL_ERR_INVALID_ARG;
    ft->tree = calloc(n + 1, sizeof(int64_t));
    if (!ft->tree) return SCL_ERR_OUT_OF_MEMORY;
    ft->n = n;
    if (data) {
        for (size_t i = 0; i < n; i++) {
            size_t idx = i + 1;
            ft->tree[idx] += data[i];
            size_t j = idx + (idx & -(int64_t)idx);
            if (j <= n) ft->tree[j] += ft->tree[idx];
        }
    }
    atomic_flag_clear(&ft->lock);
    return SCL_OK;
}

void scl_concurrent_fenwick_destroy(scl_concurrent_fenwick_t *ft)
{
    if (!ft) return;
    free(ft->tree);
    ft->tree = NULL;
    ft->n = 0;
}

scl_error_t scl_concurrent_fenwick_update(scl_concurrent_fenwick_t *ft, size_t index, int64_t delta)
{
    if (!ft) return SCL_ERR_NULL_PTR;
    if (index >= ft->n) return SCL_ERR_INVALID_INDEX;
    spin_lock(&ft->lock);
    size_t i = index + 1;
    while (i <= ft->n) {
        ft->tree[i] += delta;
        i += i & -(int64_t)i;
    }
    spin_unlock(&ft->lock);
    return SCL_OK;
}

scl_error_t scl_concurrent_fenwick_prefix(const scl_concurrent_fenwick_t *ft, size_t index, int64_t *out)
{
    if (!ft || !out) return SCL_ERR_NULL_PTR;
    if (index >= ft->n) return SCL_ERR_INVALID_INDEX;
    spin_lock((atomic_flag *)&ft->lock);
    int64_t sum = 0;
    size_t i = index + 1;
    while (i > 0) {
        sum += ft->tree[i];
        i -= i & -(int64_t)i;
    }
    *out = sum;
    spin_unlock((atomic_flag *)&ft->lock);
    return SCL_OK;
}

scl_error_t scl_concurrent_fenwick_range(const scl_concurrent_fenwick_t *ft, size_t l, size_t r,
                                         int64_t *out)
{
    if (!ft || !out) return SCL_ERR_NULL_PTR;
    if (l >= ft->n || r >= ft->n || l > r) return SCL_ERR_INVALID_INDEX;
    spin_lock((atomic_flag *)&ft->lock);
    int64_t sum_r = 0;
    size_t i = r + 1;
    while (i > 0) { sum_r += ft->tree[i]; i -= i & -(int64_t)i; }
    int64_t sum_l = 0;
    i = l;
    while (i > 0) { sum_l += ft->tree[i]; i -= i & -(int64_t)i; }
    *out = sum_r - sum_l;
    spin_unlock((atomic_flag *)&ft->lock);
    return SCL_OK;
}
