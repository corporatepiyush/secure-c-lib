#include "scl_sparse.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_sparse_init(scl_sparse_t *st, const int64_t *data, size_t n,
                            scl_sparse_op_t op)
{
    if (!st || !data || !op) return SCL_ERR_NULL_PTR;
    if (n == 0) return SCL_ERR_INVALID_ARG;

    st->n = n;
    st->op = op;

    st->k = 0;
    while ((1ULL << st->k) <= n) st->k++;

    st->table = calloc(st->k, sizeof(int64_t *));
    if (!st->table) return SCL_ERR_OUT_OF_MEMORY;

    st->table[0] = malloc(n * sizeof(int64_t));
    if (!st->table[0]) {
        free(st->table);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    memcpy(st->table[0], data, n * sizeof(int64_t));

    for (size_t j = 1; j < st->k; j++) {
        size_t len = n - (1ULL << j) + 1;
        st->table[j] = malloc(len * sizeof(int64_t));
        if (!st->table[j]) {
            for (size_t i = 0; i < j; i++) free(st->table[i]);
            free(st->table);
            return SCL_ERR_OUT_OF_MEMORY;
        }
        for (size_t i = 0; i < len; i++) {
            st->table[j][i] = op(st->table[j - 1][i],
                                 st->table[j - 1][i + (1ULL << (j - 1))]);
        }
    }

    return SCL_OK;
}

void scl_sparse_destroy(scl_sparse_t *st)
{
    if (st) {
        for (size_t i = 0; i < st->k; i++)
            free(st->table[i]);
        free(st->table);
        st->table = NULL;
        st->n = 0;
        st->k = 0;
    }
}

scl_error_t scl_sparse_query(const scl_sparse_t *st, size_t l, size_t r, int64_t *out)
{
    if (!st || !out) return SCL_ERR_NULL_PTR;
    if (l > r || r >= st->n) return SCL_ERR_INVALID_INDEX;

    size_t len = r - l + 1;
    size_t j = 0;
    while ((1ULL << (j + 1)) <= len) j++;

    *out = st->op(st->table[j][l], st->table[j][r - (1ULL << j) + 1]);
    return SCL_OK;
}
