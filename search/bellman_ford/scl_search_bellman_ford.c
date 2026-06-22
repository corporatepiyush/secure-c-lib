#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_search_bellman_ford.h"
#include <stdlib.h>
#include <limits.h>

scl_error_t scl_search_bellman_ford(const scl_graph_t *graph, int start, int64_t *restrict dist, int *restrict prev)
{
    if (__builtin_expect(graph == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(dist == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(prev == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!graph->adj, 0)) return SCL_ERR_INVALID_ARG;
    if (__builtin_expect(start < 0 || (size_t)start >= graph->vertex_count, 0)) return SCL_ERR_INVALID_INDEX;
    if (__builtin_expect(graph->vertex_count == 0, 0)) return SCL_ERR_EMPTY;

    size_t n = graph->vertex_count;
    for (size_t i = 0; i < n; i++) {
        dist[i] = INT64_MAX;
        prev[i] = -1;
    }
    dist[start] = 0;

    for (size_t iter = 0; iter < n - 1; iter++) {
        bool updated = false;
        for (size_t u = 0; u < n; u++) {
            if (dist[u] == INT64_MAX) continue;
            scl_adj_node_t *node = graph->adj[u];
            while (node) {
                int64_t nd = dist[u] + (int64_t)node->weight;
                if (nd < dist[node->to]) {
                    dist[node->to] = nd;
                    prev[node->to] = (int)u;
                    updated = true;
                }
                node = node->next;
            }
        }
        if (!updated) break;
    }

    for (size_t u = 0; u < n; u++) {
        if (dist[u] == INT64_MAX) continue;
        scl_adj_node_t *node = graph->adj[u];
        while (node) {
            int64_t nd = dist[u] + (int64_t)node->weight;
            if (nd < dist[node->to])
                return SCL_ERR_INVALID_STATE;
            node = node->next;
        }
    }
    return SCL_OK;
}
