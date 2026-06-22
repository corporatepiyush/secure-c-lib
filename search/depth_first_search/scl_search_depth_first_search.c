#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_search_depth_first_search.h"
#include <stdlib.h>

static void dfs_recursive(const scl_graph_t *graph, int v, bool *visited)
{
    visited[v] = true;
    scl_adj_node_t *node = graph->adj[v];
    while (node) {
        if (!visited[node->to])
            dfs_recursive(graph, (int)node->to, visited);
        node = node->next;
    }
}

scl_error_t scl_search_depth_first_search(const scl_graph_t *graph, int start, bool *visited)
{
    if (__builtin_expect(graph == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(visited == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!graph->adj, 0)) return SCL_ERR_INVALID_ARG;
    if (__builtin_expect(start < 0 || (size_t)start >= graph->vertex_count, 0)) return SCL_ERR_INVALID_INDEX;
    if (__builtin_expect(graph->vertex_count == 0, 0)) return SCL_ERR_EMPTY;

    dfs_recursive(graph, start, visited);
    return SCL_OK;
}
