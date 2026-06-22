#include "concurrent_segtree.h"
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

scl_error_t scl_concurrent_segtree_init(scl_concurrent_segtree_t *st, const int64_t *data, size_t n,
                                        scl_concurrent_segtree_op_t op, int64_t identity)
{
    if (!st) return SCL_ERR_NULL_PTR;
    if (n == 0 || !op) return SCL_ERR_INVALID_ARG;
    st->n = n;
    st->op = op;
    st->identity = identity;
    size_t size = 1;
    while (size < n) size *= 2;
    st->size = size;
    st->tree = calloc(2 * size, sizeof(int64_t));
    if (!st->tree) return SCL_ERR_OUT_OF_MEMORY;
    if (data) {
        for (size_t i = 0; i < n; i++)
            st->tree[size + i] = data[i];
        for (size_t i = n; i < size; i++)
            st->tree[size + i] = identity;
        for (size_t i = 2 * size - 1; i > 1; i--)
            st->tree[i / 2] = op(st->tree[i / 2], st->tree[i]);
    } else {
        for (size_t i = 1; i < 2 * size; i++)
            st->tree[i] = identity;
    }
    atomic_flag_clear(&st->lock);
    return SCL_OK;
}

void scl_concurrent_segtree_destroy(scl_concurrent_segtree_t *st)
{
    if (!st) return;
    free(st->tree);
    st->tree = NULL;
    st->n = 0;
    st->size = 0;
}

scl_error_t scl_concurrent_segtree_update(scl_concurrent_segtree_t *st, size_t index, int64_t value)
{
    if (!st) return SCL_ERR_NULL_PTR;
    if (index >= st->n) return SCL_ERR_INVALID_INDEX;
    spin_lock(&st->lock);
    size_t i = st->size + index;
    st->tree[i] = value;
    for (i /= 2; i > 0; i /= 2)
        st->tree[i] = st->op(st->tree[2 * i], st->tree[2 * i + 1]);
    spin_unlock(&st->lock);
    return SCL_OK;
}

scl_error_t scl_concurrent_segtree_query(const scl_concurrent_segtree_t *st, size_t l, size_t r,
                                         int64_t *out)
{
    if (!st || !out) return SCL_ERR_NULL_PTR;
    if (l >= st->n || r >= st->n || l > r) return SCL_ERR_INVALID_INDEX;
    spin_lock((atomic_flag *)&st->lock);
    int64_t left_res = st->identity;
    int64_t right_res = st->identity;
    size_t l_idx = st->size + l;
    size_t r_idx = st->size + r;
    while (l_idx <= r_idx) {
        if (l_idx & 1) left_res = st->op(left_res, st->tree[l_idx++]);
        if (!(r_idx & 1)) right_res = st->op(st->tree[r_idx--], right_res);
        l_idx /= 2;
        r_idx /= 2;
    }
    *out = st->op(left_res, right_res);
    spin_unlock((atomic_flag *)&st->lock);
    return SCL_OK;
}
