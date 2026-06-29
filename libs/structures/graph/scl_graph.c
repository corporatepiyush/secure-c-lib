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

/* graph data structure. */

#include "scl_graph.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_graph_init(scl_allocator_t *alloc, scl_graph_t *g, size_t vertex_count)
{
    if (scl_unlikely(!g)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(vertex_count == 0)) return SCL_ERR_INVALID_ARG;

    /* One zeroed shard descriptor per vertex (edges=NULL, count=cap=0). */
    g->adj = scl_calloc(alloc, vertex_count, sizeof(scl_adj_list_t), alignof(max_align_t));
    if (scl_unlikely(!g->adj)) return SCL_ERR_OUT_OF_MEMORY;

    g->vertex_count = vertex_count;
    g->edge_count = 0;
    return SCL_OK;
}

void scl_graph_destroy(scl_allocator_t *alloc, scl_graph_t *g)
{
    if (scl_unlikely(!g)) return;
    if (g->adj) {
        for (size_t i = 0; i < g->vertex_count; i++)
            scl_free(alloc, g->adj[i].edges);   /* free each vertex's shard */
        scl_free(alloc, g->adj);
    }
    g->adj = NULL;
    g->vertex_count = 0;
    g->edge_count = 0;
}

scl_error_t scl_graph_add_edge(scl_allocator_t *alloc, scl_graph_t *g, size_t from, size_t to, int weight)
{
    if (scl_unlikely(!g)) return SCL_ERR_NULL_PTR;
    if (from >= g->vertex_count || to >= g->vertex_count)
        return SCL_ERR_INVALID_INDEX;

    scl_adj_list_t *l = &g->adj[from];
    if (l->count == l->cap) {
        size_t ncap = l->cap ? l->cap * 2 : 4;
        size_t nbytes;
        if (scl_unlikely(scl_mul_overflow(ncap, sizeof(scl_adj_entry_t), &nbytes)))
            return SCL_ERR_SIZE_OVERFLOW;
        scl_adj_entry_t *ne = scl_realloc(alloc, l->edges,
                                          l->cap * sizeof(scl_adj_entry_t), nbytes,
                                          alignof(max_align_t));
        if (scl_unlikely(!ne)) return SCL_ERR_OUT_OF_MEMORY;
        l->edges = ne;
        l->cap = ncap;
    }
    l->edges[l->count].to = to;
    l->edges[l->count].weight = weight;
    l->count++;
    g->edge_count++;
    return SCL_OK;
}

scl_error_t scl_graph_remove_edge(scl_allocator_t *alloc, scl_graph_t *g, size_t from, size_t to)
{
    (void)alloc;
    if (scl_unlikely(!g)) return SCL_ERR_NULL_PTR;
    if (from >= g->vertex_count || to >= g->vertex_count)
        return SCL_ERR_INVALID_INDEX;

    scl_adj_list_t *l = &g->adj[from];
    for (size_t i = 0; i < l->count; i++) {
        if (l->edges[i].to == to) {
            /* O(1) removal: move the last entry into the hole (edge order within
             * a vertex is not significant for any graph algorithm). */
            l->edges[i] = l->edges[l->count - 1];
            l->count--;
            g->edge_count--;
            return SCL_OK;
        }
    }
    return SCL_ERR_NOT_FOUND;
}

bool scl_graph_has_edge(const scl_graph_t *g, size_t from, size_t to)
{
    if (!g || from >= g->vertex_count || to >= g->vertex_count)
        return false;
    const scl_adj_list_t *l = &g->adj[from];
    for (size_t i = 0; i < l->count; i++)
        if (l->edges[i].to == to) return true;
    return false;
}

size_t scl_graph_vertex_count(const scl_graph_t *g) { return g ? g->vertex_count : 0; }
size_t scl_graph_edge_count(const scl_graph_t *g) { return g ? g->edge_count : 0; }

scl_error_t scl_graph_dfs(scl_allocator_t *alloc, const scl_graph_t *g, size_t start,
                          void (*visit)(size_t, void *), void  *SCL_RESTRICT ctx)
{
    if (scl_unlikely(!g || !visit)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(start >= g->vertex_count)) return SCL_ERR_INVALID_INDEX;

    bool *visited = scl_calloc(alloc, g->vertex_count, sizeof(bool), alignof(max_align_t));
    if (scl_unlikely(!visited)) return SCL_ERR_OUT_OF_MEMORY;

    size_t stack_sz;
    if (scl_mul_overflow(g->vertex_count, sizeof(size_t), &stack_sz)) {
        scl_free(alloc, visited);
        return SCL_ERR_SIZE_OVERFLOW;
    }
    size_t *stack = scl_alloc(alloc, stack_sz, alignof(max_align_t));
    if (!stack) { scl_free(alloc, visited); return SCL_ERR_OUT_OF_MEMORY; }

    /* Mark on push so each vertex is pushed at most once — otherwise a vertex
     * with many in-edges could be pushed more than vertex_count times and
     * overflow the V-sized stack (a heap buffer overflow on any graph with
     * E > V). Visit order is preorder of this push-marking traversal. */
    size_t sp = 0;
    visited[start] = true;
    stack[sp++] = start;

    while (sp > 0) {
        size_t v = stack[--sp];
        visit(v, ctx);
        const scl_adj_list_t *l = &g->adj[v];
        for (size_t i = 0; i < l->count; i++) {
            size_t to = l->edges[i].to;
            if (!visited[to]) {
                visited[to] = true;
                stack[sp++] = to;
            }
        }
    }

    scl_free(alloc, stack);
    scl_free(alloc, visited);
    return SCL_OK;
}

scl_error_t scl_graph_bfs(scl_allocator_t *alloc, const scl_graph_t *g, size_t start,
                          void (*visit)(size_t, void *), void  *SCL_RESTRICT ctx)
{
    if (scl_unlikely(!g || !visit)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(start >= g->vertex_count)) return SCL_ERR_INVALID_INDEX;

    bool *visited = scl_calloc(alloc, g->vertex_count, sizeof(bool), alignof(max_align_t));
    if (scl_unlikely(!visited)) return SCL_ERR_OUT_OF_MEMORY;

    size_t queue_sz;
    if (scl_mul_overflow(g->vertex_count, sizeof(size_t), &queue_sz)) {
        scl_free(alloc, visited);
        return SCL_ERR_SIZE_OVERFLOW;
    }
    size_t *queue = scl_alloc(alloc, queue_sz, alignof(max_align_t));
    if (!queue) { scl_free(alloc, visited); return SCL_ERR_OUT_OF_MEMORY; }

    size_t qh = 0, qt = 0;
    visited[start] = true;
    queue[qt++] = start;

    while (qh < qt) {
        size_t v = queue[qh++];
        visit(v, ctx);
        const scl_adj_list_t *l = &g->adj[v];
        for (size_t i = 0; i < l->count; i++) {
            size_t to = l->edges[i].to;
            if (!visited[to]) {
                visited[to] = true;
                queue[qt++] = to;
            }
        }
    }

    scl_free(alloc, queue);
    scl_free(alloc, visited);
    return SCL_OK;
}
