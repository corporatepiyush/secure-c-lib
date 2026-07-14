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

/* Bellman-Ford SSSP. O(VE). Negative-weight edges. Detects negative cycles. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_bellman_ford.h"
#include "scl_graph.h"
#include "scl_limits.h"
#include "scl_stdlib.h"

scl_error_t scl_search_bellman_ford(const scl_graph_t *graph, int start,
                                    int64_t *dist, int *prev) {
  if (scl_unlikely(graph == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(dist == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(prev == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(!graph->nodes.data))
    return SCL_ERR_INVALID_ARG;
  if (scl_unlikely(start < 0 || (size_t)start >= graph->vertex_count))
    return SCL_ERR_INVALID_INDEX;
  if (scl_unlikely(graph->vertex_count == 0))
    return SCL_ERR_EMPTY;

  size_t n = graph->vertex_count;
  for (size_t i = 0; i < n; i++) {
    dist[i] = INT64_MAX;
    prev[i] = -1;
  }
  dist[start] = 0;

  for (size_t iter = 0; iter < n - 1; iter++) {
    bool updated = false;
    for (size_t u = 0; u < n; u++) {
      if (dist[u] == INT64_MAX)
        continue;
      for (size_t e = scl_graph_adj_head(graph, u); e != SCL_GRAPH_NIL;) {
        const scl_graph_edge_t *ed = scl_graph_edge(graph, e);
        int64_t nd;
        /* Saturating add: with negative weights dist[u] can be very
         * negative, so dist[u]+weight may overflow int64 (signed
         * overflow is UB). Skip the relaxation if it would overflow. */
        if (!__builtin_add_overflow(dist[u], (int64_t)ed->weight, &nd) &&
            nd < dist[ed->to]) {
          dist[ed->to] = nd;
          prev[ed->to] = (int)u;
          updated = true;
        }
        e = ed->next;
      }
    }
    if (!updated)
      break;
  }

  for (size_t u = 0; u < n; u++) {
    if (dist[u] == INT64_MAX)
      continue;
    for (size_t e = scl_graph_adj_head(graph, u); e != SCL_GRAPH_NIL;) {
      const scl_graph_edge_t *ed = scl_graph_edge(graph, e);
      int64_t nd;
      if (!__builtin_add_overflow(dist[u], (int64_t)ed->weight, &nd) &&
          nd < dist[ed->to])
        return SCL_ERR_INVALID_STATE;
      e = ed->next;
    }
  }
  return SCL_OK;
}
