#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_search_breadth_first_search.h"
#include <stdlib.h>

scl_error_t scl_search_breadth_first_search(const scl_graph_t *graph, int start, bool *visited)
{
    if (__builtin_expect(graph == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(visited == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!graph->adj, 0)) return SCL_ERR_INVALID_ARG;
    if (__builtin_expect(start < 0 || (size_t)start >= graph->vertex_count, 0)) return SCL_ERR_INVALID_INDEX;
    if (__builtin_expect(graph->vertex_count == 0, 0)) return SCL_ERR_EMPTY;

    int n = (int)graph->vertex_count;
    int *queue = (int *)malloc((size_t)n * sizeof(int));
    if (__builtin_expect(queue == NULL, 0)) return SCL_ERR_OUT_OF_MEMORY;

    int front = 0, back = 0;
    visited[start] = true;
    queue[back++] = start;

    while (front < back) {
        int v = queue[front++];
        scl_adj_node_t *node = graph->adj[v];
        while (node) {
            if (!visited[node->to]) {
                visited[node->to] = true;
                queue[back++] = (int)node->to;
            }
            node = node->next;
        }
    }
    free(queue);
    return SCL_OK;
}
