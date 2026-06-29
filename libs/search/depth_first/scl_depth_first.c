/*
 * Copyright 2026 Piyush Katariya
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* DFS traversal. O(V+E). Stack/recursive. Topological sort, cycle detection. */

#include "scl_depth_first.h"
#include "scl_graph.h"

scl_error_t scl_search_depth_first_search(scl_allocator_t * alloc, const scl_graph_t * graph, int start, bool * visited)
{
    if (scl_unlikely(graph == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(visited == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!graph->nodes.shards)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(start < 0 || (size_t)start >= graph->vertex_count)) return SCL_ERR_INVALID_INDEX;
    if (scl_unlikely(graph->vertex_count == 0)) return SCL_ERR_EMPTY;

    size_t n = graph->vertex_count;
    size_t bytes;
    if (scl_unlikely(scl_mul_overflow(n, sizeof(int), &bytes))) return SCL_ERR_SIZE_OVERFLOW;
    int *stack = (int *)scl_alloc(alloc, bytes, alignof(max_align_t));
    if (scl_unlikely(stack == NULL)) return SCL_ERR_OUT_OF_MEMORY;

    /* Mark vertices as they are PUSHED, not when popped. The previous version
     * marked on pop, so a vertex with k in-edges could be pushed k times before
     * being popped — the V-sized stack then overflowed for any graph with more
     * edges than vertices (a heap buffer overflow). Marking on push guarantees
     * each vertex is pushed at most once, bounding the stack to V. The set of
     * reachable vertices reported in `visited` is identical. */
    int sp = 0;
    visited[start] = true;
    stack[sp++] = start;

    while (sp > 0) {
        int v = stack[--sp];
        for (size_t e = scl_graph_adj_head(graph, (size_t)v); e != SCL_GRAPH_NIL; ) {
            const scl_graph_edge_t *ed = scl_graph_edge(graph, e);
            if (!visited[ed->to]) {
                visited[ed->to] = true;
                stack[sp++] = (int)ed->to;
            }
            e = ed->next;
        }
    }

    scl_free(alloc, stack);
    return SCL_OK;
}
