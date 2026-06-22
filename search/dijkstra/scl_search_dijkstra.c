#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_search_dijkstra.h"
#include <stdlib.h>
#include <limits.h>

scl_error_t scl_search_dijkstra(const scl_graph_t *graph, int start, int64_t *restrict dist, int *restrict prev)
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

    bool *visited = (bool *)calloc(n, sizeof(bool));
    if (__builtin_expect(visited == NULL, 0)) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t count = 0; count < n; count++) {
        size_t u = n;
        int64_t min_dist = INT64_MAX;
        for (size_t i = 0; i < n; i++) {
            if (!visited[i] && dist[i] < min_dist) {
                min_dist = dist[i];
                u = i;
            }
        }
        if (u == n || min_dist == INT64_MAX) break;
        visited[u] = true;

        scl_adj_node_t *node = graph->adj[u];
        while (node) {
            if (!visited[node->to]) {
                int64_t nd = dist[u] + (int64_t)node->weight;
                if (nd < dist[node->to]) {
                    dist[node->to] = nd;
                    prev[node->to] = (int)u;
                }
            }
            node = node->next;
        }
    }
    free(visited);
    return SCL_OK;
}
