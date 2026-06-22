#include "scl_segtree.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_segtree_init(scl_segtree_t *st, const int64_t *data, size_t n,
                             scl_segtree_op_t op, int64_t identity)
{
    if (!st || !data || !op) return SCL_ERR_NULL_PTR;
    if (n == 0) return SCL_ERR_INVALID_ARG;

    st->n = n;
    st->op = op;
    st->identity = identity;

    size_t size = 1;
    while (size < n) size *= 2;
    st->size = size;

    st->tree = calloc(2 * size, sizeof(int64_t));
    if (!st->tree) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t i = 0; i < n; i++)
        st->tree[size + i] = data[i];
    for (size_t i = n; i < size; i++)
        st->tree[size + i] = identity;

    for (size_t i = size - 1; i > 0; i--)
        st->tree[i] = op(st->tree[2 * i], st->tree[2 * i + 1]);

    return SCL_OK;
}

void scl_segtree_destroy(scl_segtree_t *st)
{
    if (st) {
        free(st->tree);
        st->tree = NULL;
        st->n = 0;
        st->size = 0;
    }
}

scl_error_t scl_segtree_update(scl_segtree_t *st, size_t index, int64_t value)
{
    if (!st) return SCL_ERR_NULL_PTR;
    if (index >= st->n) return SCL_ERR_INVALID_INDEX;

    size_t pos = st->size + index;
    st->tree[pos] = value;
    pos /= 2;

    while (pos > 0) {
        st->tree[pos] = st->op(st->tree[2 * pos], st->tree[2 * pos + 1]);
        pos /= 2;
    }
    return SCL_OK;
}

scl_error_t scl_segtree_query(const scl_segtree_t *st, size_t l, size_t r, int64_t *out)
{
    if (!st || !out) return SCL_ERR_NULL_PTR;
    if (l > r || r >= st->n) return SCL_ERR_INVALID_INDEX;

    int64_t res_left = st->identity;
    int64_t res_right = st->identity;

    size_t left = st->size + l;
    size_t right = st->size + r + 1;

    while (left < right) {
        if (left & 1)
            res_left = st->op(res_left, st->tree[left++]);
        if (right & 1)
            res_right = st->op(st->tree[--right], res_right);
        left /= 2;
        right /= 2;
    }

    *out = st->op(res_left, res_right);
    return SCL_OK;
}
