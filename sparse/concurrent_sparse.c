#include "concurrent_sparse.h"
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

scl_error_t scl_concurrent_sparse_init(scl_concurrent_sparse_t *st, const int64_t *data, size_t n,
                                       scl_concurrent_sparse_op_t op)
{
    if (!st) return SCL_ERR_NULL_PTR;
    if (n == 0 || !op || !data) return SCL_ERR_INVALID_ARG;
    st->n = n;
    st->op = op;
    size_t k = 1;
    while ((1ULL << k) <= n) k++;
    st->k = k;
    st->table = malloc(k * sizeof(int64_t *));
    if (!st->table) return SCL_ERR_OUT_OF_MEMORY;
    st->table[0] = malloc(n * sizeof(int64_t));
    if (!st->table[0]) { free(st->table); return SCL_ERR_OUT_OF_MEMORY; }
    memcpy(st->table[0], data, n * sizeof(int64_t));
    for (size_t j = 1; j < k; j++) {
        st->table[j] = malloc(n * sizeof(int64_t));
        if (!st->table[j]) {
            for (size_t i = 0; i < j; i++) free(st->table[i]);
            free(st->table);
            return SCL_ERR_OUT_OF_MEMORY;
        }
        size_t step = 1ULL << (j - 1);
        for (size_t i = 0; i + (1ULL << j) <= n; i++)
            st->table[j][i] = op(st->table[j - 1][i], st->table[j - 1][i + step]);
    }
    atomic_flag_clear(&st->lock);
    return SCL_OK;
}

void scl_concurrent_sparse_destroy(scl_concurrent_sparse_t *st)
{
    if (!st) return;
    for (size_t j = 0; j < st->k; j++)
        free(st->table[j]);
    free(st->table);
    st->table = NULL;
    st->n = 0;
    st->k = 0;
}

scl_error_t scl_concurrent_sparse_query(const scl_concurrent_sparse_t *st, size_t l, size_t r,
                                        int64_t *out)
{
    if (!st || !out) return SCL_ERR_NULL_PTR;
    if (l >= st->n || r >= st->n || l > r) return SCL_ERR_INVALID_INDEX;
    spin_lock((atomic_flag *)&st->lock);
    size_t len = r - l + 1;
    int64_t res;
    bool first = true;
    for (size_t bit = 0; (1ULL << bit) <= len; bit++) {
        if (len & (1ULL << bit)) {
            if (first) {
                res = st->table[bit][l];
                first = false;
            } else {
                res = st->op(res, st->table[bit][l]);
            }
            l += (1ULL << bit);
        }
    }
    *out = res;
    spin_unlock((atomic_flag *)&st->lock);
    return SCL_OK;
}
