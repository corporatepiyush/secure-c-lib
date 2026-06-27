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

/* Thread-safe graph data structure. Guarded by scl_spinlock_t. */

#include "scl_concurrent_graph.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_cgraph_init(scl_allocator_t *alloc, scl_concurrent_graph_t *g, size_t vertex_count)
{
    if (scl_unlikely(!g)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(vertex_count == 0)) return SCL_ERR_INVALID_ARG;
    g->adj = scl_calloc(alloc, vertex_count, sizeof(scl_concurrent_adj_node_t *), alignof(max_align_t));
    if (scl_unlikely(!g->adj)) return SCL_ERR_OUT_OF_MEMORY;
    g->vertex_count = vertex_count;
    atomic_init(&g->edge_count, 0);
    scl_spinlock_init(&g->lock);
    return SCL_OK;
}

void scl_cgraph_destroy(scl_allocator_t *alloc, scl_concurrent_graph_t *g)
{
    if (scl_unlikely(!g)) return;
    scl_spinlock_lock(&g->lock);
    for (size_t i = 0; i < g->vertex_count; i++) {
        scl_concurrent_adj_node_t *cur = g->adj[i];
        while (scl_likely(cur)) {
            scl_concurrent_adj_node_t *next = cur->next;
            scl_free(alloc, cur);
            cur = next;
        }
    }
    scl_free(alloc, g->adj);
    g->adj = NULL;
    g->vertex_count = 0;
    atomic_store_explicit(&g->edge_count, 0, memory_order_relaxed);
    scl_spinlock_unlock(&g->lock);
}

scl_error_t scl_cgraph_add_edge(scl_allocator_t *alloc, scl_concurrent_graph_t *g, size_t from, size_t to, int weight)
{
    if (scl_unlikely(!g)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(from >= g->vertex_count || to >= g->vertex_count)) return SCL_ERR_INVALID_INDEX;
    scl_concurrent_adj_node_t *node = scl_alloc(alloc, sizeof(scl_concurrent_adj_node_t), alignof(max_align_t));
    if (scl_unlikely(!node)) return SCL_ERR_OUT_OF_MEMORY;
    node->to = to;
    node->weight = weight;
    scl_spinlock_lock(&g->lock);
    node->next = g->adj[from];
    g->adj[from] = node;
    atomic_fetch_add_explicit(&g->edge_count, 1, memory_order_relaxed);
    scl_spinlock_unlock(&g->lock);
    return SCL_OK;
}

scl_error_t scl_cgraph_remove_edge(scl_allocator_t *alloc, scl_concurrent_graph_t *g, size_t from, size_t to)
{
    if (scl_unlikely(!g)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(from >= g->vertex_count)) return SCL_ERR_INVALID_INDEX;
    scl_spinlock_lock(&g->lock);
    scl_concurrent_adj_node_t **prev = &g->adj[from];
    scl_concurrent_adj_node_t *cur = g->adj[from];
    while (scl_likely(cur)) {
        if (cur->to == to) {
            *prev = cur->next;
            scl_free(alloc, cur);
            atomic_fetch_sub_explicit(&g->edge_count, 1, memory_order_relaxed);
            scl_spinlock_unlock(&g->lock);
            return SCL_OK;
        }
        prev = &cur->next;
        cur = cur->next;
    }
    scl_spinlock_unlock(&g->lock);
    return SCL_ERR_NOT_FOUND;
}

bool scl_cgraph_has_edge(const scl_concurrent_graph_t *g, size_t from, size_t to)
{
    if (scl_unlikely(!g || from >= g->vertex_count)) return false;
    scl_spinlock_lock((scl_spinlock_t *)&g->lock);
    scl_concurrent_adj_node_t *cur = g->adj[from];
    while (scl_likely(cur)) {
        if (cur->to == to) { scl_spinlock_unlock((scl_spinlock_t *)&g->lock); return true; }
        cur = cur->next;
    }
    scl_spinlock_unlock((scl_spinlock_t *)&g->lock);
    return false;
}
