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

/* Dijkstra's SSSP. O((V+E) log V). Non-negative weights. Greedy relaxation. */

#include "scl_dijkstra.h"
#include <limits.h>

scl_error_t scl_search_dijkstra(scl_allocator_t * alloc, const scl_graph_t * graph, int start, int64_t * dist, int * prev)
{
    if (scl_unlikely(graph == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(dist == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(prev == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!graph->adj)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(start < 0 || (size_t)start >= graph->vertex_count)) return SCL_ERR_INVALID_INDEX;
    if (scl_unlikely(graph->vertex_count == 0)) return SCL_ERR_EMPTY;

    size_t n = graph->vertex_count;
    for (size_t i = 0; i < n; i++) {
        dist[i] = INT64_MAX;
        prev[i] = -1;
    }
    dist[start] = 0;

    bool *visited = (bool *)scl_calloc(alloc, n, sizeof(bool), alignof(max_align_t));
    if (scl_unlikely(visited == NULL)) return SCL_ERR_OUT_OF_MEMORY;

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
    scl_free(alloc, visited);
    return SCL_OK;
}
