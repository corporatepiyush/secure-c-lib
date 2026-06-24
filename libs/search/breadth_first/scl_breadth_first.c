#include "scl_breadth_first.h"
#include <stdlib.h>

scl_error_t scl_search_breadth_first_search(scl_allocator_t * alloc, const scl_graph_t * graph, int start, bool * visited)
{
    if (scl_unlikely(graph == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(visited == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!graph->adj)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(start < 0 || (size_t)start >= graph->vertex_count)) return SCL_ERR_INVALID_INDEX;
    if (scl_unlikely(graph->vertex_count == 0)) return SCL_ERR_EMPTY;

    size_t n = graph->vertex_count;
    int *queue = (int *)scl_alloc(alloc, n * sizeof(int), alignof(max_align_t));
    if (scl_unlikely(queue == NULL)) return SCL_ERR_OUT_OF_MEMORY;

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
    scl_free(alloc, queue);
    return SCL_OK;
}
