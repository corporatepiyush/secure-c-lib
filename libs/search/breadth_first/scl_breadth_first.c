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

/* BFS traversal. O(V+E). Queue-based level-order. Shortest path on unweighted.
 */

#include "scl_breadth_first.h"
#include "scl_graph.h"
#include "scl_stdlib.h"

scl_error_t scl_search_breadth_first_search(scl_allocator_t *alloc,
                                            const scl_graph_t *graph, int start,
                                            bool *visited) {
  if (scl_unlikely(graph == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(visited == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(!graph->nodes.data))
    return SCL_ERR_INVALID_ARG;
  if (scl_unlikely(start < 0 || (size_t)start >= graph->vertex_count))
    return SCL_ERR_INVALID_INDEX;
  if (scl_unlikely(graph->vertex_count == 0))
    return SCL_ERR_EMPTY;

  size_t n = graph->vertex_count;
  size_t bytes;
  if (scl_unlikely(scl_mul_overflow(n, sizeof(int), &bytes)))
    return SCL_ERR_SIZE_OVERFLOW;
  int *queue = (int *)scl_alloc(alloc, bytes, alignof(max_align_t));
  if (scl_unlikely(queue == NULL))
    return SCL_ERR_OUT_OF_MEMORY;

  int front = 0, back = 0;
  visited[start] = true;
  queue[back++] = start;

  while (front < back) {
    int v = queue[front++];
    for (size_t e = scl_graph_adj_head(graph, (size_t)v); e != SCL_GRAPH_NIL;) {
      const scl_graph_edge_t *ed = scl_graph_edge(graph, e);
      if (!visited[ed->to]) {
        visited[ed->to] = true;
        queue[back++] = (int)ed->to;
      }
      e = ed->next;
    }
  }
  scl_free(alloc, queue);
  return SCL_OK;
}
