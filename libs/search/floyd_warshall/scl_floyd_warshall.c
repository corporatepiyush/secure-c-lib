#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_floyd_warshall.h"
#include <limits.h>
#include <stdlib.h>

scl_error_t scl_search_floyd_warshall(int n, const scl_edge_t *edges, size_t ecount, int64_t *restrict dist)
{
    if (__builtin_expect(edges == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(dist == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(n <= 0, 0)) return SCL_ERR_EMPTY;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            dist[i * n + j] = (i == j) ? 0 : INT64_MAX;
        }
    }
    for (size_t e = 0; e < ecount; e++) {
        int f = (int)edges[e].from;
        int t = (int)edges[e].to;
        if (f >= 0 && f < n && t >= 0 && t < n)
            dist[f * n + t] = (int64_t)edges[e].weight;
    }

    for (int k = 0; k < n; k++) {
        for (int i = 0; i < n; i++) {
            if (dist[i * n + k] == INT64_MAX) continue;
            for (int j = 0; j < n; j++) {
                if (dist[k * n + j] == INT64_MAX) continue;
                int64_t nd = dist[i * n + k] + dist[k * n + j];
                if (nd < dist[i * n + j])
                    dist[i * n + j] = nd;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        if (dist[i * n + i] < 0)
            return SCL_ERR_INVALID_STATE;
    }
    return SCL_OK;
}
