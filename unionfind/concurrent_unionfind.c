#include "concurrent_unionfind.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_concurrent_unionfind_init(scl_concurrent_unionfind_t *uf, size_t count)
{
    if (!uf) return SCL_ERR_NULL_PTR;
    if (count == 0) return SCL_ERR_INVALID_ARG;
    uf->parent = malloc(count * sizeof(atomic_uint));
    if (!uf->parent) return SCL_ERR_OUT_OF_MEMORY;
    uf->rank = malloc(count * sizeof(atomic_uint));
    if (!uf->rank) {
        free(uf->parent);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < count; i++) {
        atomic_init(&uf->parent[i], (unsigned int)i);
        atomic_init(&uf->rank[i], 0);
    }
    atomic_init(&uf->sets, (unsigned int)count);
    uf->count = count;
    return SCL_OK;
}

void scl_concurrent_unionfind_destroy(scl_concurrent_unionfind_t *uf)
{
    if (!uf) return;
    free(uf->parent);
    free(uf->rank);
    uf->parent = NULL;
    uf->rank = NULL;
    uf->count = 0;
    atomic_store_explicit(&uf->sets, 0, memory_order_relaxed);
}

size_t scl_concurrent_unionfind_find(scl_concurrent_unionfind_t *uf, size_t x)
{
    if (!uf || x >= uf->count) return (size_t)-1;
    while (1) {
        unsigned int p = atomic_load_explicit(&uf->parent[x], memory_order_acquire);
        if (p == (unsigned int)x) return x;
        unsigned int gp = atomic_load_explicit(&uf->parent[p], memory_order_acquire);
        atomic_compare_exchange_strong_explicit(&uf->parent[x], &p, gp,
            memory_order_release, memory_order_relaxed);
        x = p;
    }
}

scl_error_t scl_concurrent_unionfind_union(scl_concurrent_unionfind_t *uf, size_t x, size_t y)
{
    if (!uf) return SCL_ERR_NULL_PTR;
    if (x >= uf->count || y >= uf->count) return SCL_ERR_INVALID_INDEX;
    while (1) {
        size_t rx = scl_concurrent_unionfind_find(uf, x);
        size_t ry = scl_concurrent_unionfind_find(uf, y);
        if (rx == ry) return SCL_OK;
        unsigned int rank_rx = atomic_load_explicit(&uf->rank[rx], memory_order_relaxed);
        unsigned int rank_ry = atomic_load_explicit(&uf->rank[ry], memory_order_relaxed);
        if (rank_rx < rank_ry) {
            if (atomic_compare_exchange_strong_explicit(&uf->parent[rx], (unsigned int *)&rx, (unsigned int)ry,
                    memory_order_release, memory_order_relaxed)) {
                atomic_fetch_sub_explicit(&uf->sets, 1, memory_order_relaxed);
                return SCL_OK;
            }
        } else if (rank_rx > rank_ry) {
            if (atomic_compare_exchange_strong_explicit(&uf->parent[ry], (unsigned int *)&ry, (unsigned int)rx,
                    memory_order_release, memory_order_relaxed)) {
                atomic_fetch_sub_explicit(&uf->sets, 1, memory_order_relaxed);
                return SCL_OK;
            }
        } else {
            if (atomic_compare_exchange_strong_explicit(&uf->parent[ry], (unsigned int *)&ry, (unsigned int)rx,
                    memory_order_release, memory_order_relaxed)) {
                atomic_fetch_add_explicit(&uf->rank[rx], 1, memory_order_relaxed);
                atomic_fetch_sub_explicit(&uf->sets, 1, memory_order_relaxed);
                return SCL_OK;
            }
        }
    }
}

bool scl_concurrent_unionfind_connected(scl_concurrent_unionfind_t *uf, size_t x, size_t y)
{
    if (!uf || x >= uf->count || y >= uf->count) return false;
    return scl_concurrent_unionfind_find(uf, x) == scl_concurrent_unionfind_find(uf, y);
}

size_t scl_concurrent_unionfind_count(const scl_concurrent_unionfind_t *uf)
{
    return uf ? uf->count : 0;
}

size_t scl_concurrent_unionfind_sets(const scl_concurrent_unionfind_t *uf)
{
    return uf ? atomic_load_explicit(&uf->sets, memory_order_relaxed) : 0;
}
