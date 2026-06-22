#ifndef SCL_SEGTREE_H
#define SCL_SEGTREE_H

#include "../common/scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef int64_t (*scl_segtree_op_t)(int64_t a, int64_t b);

typedef struct {
    int64_t *tree;
    size_t n;
    size_t size;
    scl_segtree_op_t op;
    int64_t identity;
} scl_segtree_t;

scl_error_t scl_segtree_init(scl_segtree_t *st, const int64_t *data, size_t n,
                             scl_segtree_op_t op, int64_t identity) SCL_WARN_UNUSED;
void        scl_segtree_destroy(scl_segtree_t *st);
scl_error_t scl_segtree_update(scl_segtree_t *st, size_t index, int64_t value) SCL_WARN_UNUSED;
scl_error_t scl_segtree_query(const scl_segtree_t *st, size_t l, size_t r,
                              int64_t *out) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
