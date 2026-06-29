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

/*
 * Binary min-heap with lazy deletion over (distance, vertex). The previous
 * implementation scanned all V vertices to find the minimum each step — that is
 * O(V^2), not the O((V+E) log V) the header advertises. With a heap, each edge
 * relaxation pushes at most once, so the heap never holds more than E+1 entries.
 * Stale entries (a vertex pushed again after its distance improved) are skipped
 * on pop by comparing the popped key against the current best distance.
 */
typedef struct { int64_t d; size_t v; } dj_heap_entry_t;

static void dj_sift_up(dj_heap_entry_t *h, size_t i) {
    while (i > 0) {
        size_t p = (i - 1) / 2;
        if (h[p].d <= h[i].d) break;
        dj_heap_entry_t t = h[p]; h[p] = h[i]; h[i] = t;
        i = p;
    }
}

static void dj_sift_down(dj_heap_entry_t *h, size_t n, size_t i) {
    for (;;) {
        size_t l = 2 * i + 1, r = 2 * i + 2, m = i;
        if (l < n && h[l].d < h[m].d) m = l;
        if (r < n && h[r].d < h[m].d) m = r;
        if (m == i) break;
        dj_heap_entry_t t = h[m]; h[m] = h[i]; h[i] = t;
        i = m;
    }
}

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

    /* At most one heap push per edge relaxation, plus the initial start push. */
    size_t cap, capbytes;
    if (scl_unlikely(scl_add_overflow(graph->edge_count, 1, &cap)))
        return SCL_ERR_SIZE_OVERFLOW;
    if (scl_unlikely(scl_mul_overflow(cap, sizeof(dj_heap_entry_t), &capbytes)))
        return SCL_ERR_SIZE_OVERFLOW;
    dj_heap_entry_t *heap = (dj_heap_entry_t *)scl_alloc(alloc, capbytes, alignof(dj_heap_entry_t));
    if (scl_unlikely(heap == NULL)) return SCL_ERR_OUT_OF_MEMORY;

    size_t hn = 0;
    heap[hn].d = 0; heap[hn].v = (size_t)start; hn++;

    scl_error_t result = SCL_OK;
    while (hn > 0) {
        dj_heap_entry_t top = heap[0];
        heap[0] = heap[--hn];
        dj_sift_down(heap, hn, 0);

        if (top.d > dist[top.v]) continue;   /* stale entry: already improved */

        const scl_adj_list_t *l = &graph->adj[top.v];
        for (size_t ei = 0; ei < l->count; ei++) {
            int w = l->edges[ei].weight;
            size_t to = l->edges[ei].to;
            if (scl_unlikely(w < 0)) {            /* Dijkstra needs w >= 0 */
                result = SCL_ERR_INVALID_ARG;
                goto done;
            }
            int64_t nd;
            if (!__builtin_add_overflow(dist[top.v], (int64_t)w, &nd) && nd < dist[to]) {
                dist[to] = nd;
                prev[to] = (int)top.v;
                if (scl_likely(hn < cap)) {       /* bound holds; defensive */
                    heap[hn].d = nd; heap[hn].v = to;
                    dj_sift_up(heap, hn);
                    hn++;
                }
            }
        }
    }

done:
    scl_free(alloc, heap);
    return result;
}
