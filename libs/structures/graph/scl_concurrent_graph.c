#include "scl_concurrent_graph.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_cgraph_init(scl_allocator_t *alloc, scl_concurrent_graph_t *g, size_t vertex_count)
{
    if (!g) return SCL_ERR_NULL_PTR;
    if (vertex_count == 0) return SCL_ERR_INVALID_ARG;
    g->adj = scl_calloc(alloc, vertex_count, sizeof(scl_concurrent_adj_node_t *), alignof(max_align_t));
    if (!g->adj) return SCL_ERR_OUT_OF_MEMORY;
    g->vertex_count = vertex_count;
    atomic_init(&g->edge_count, 0);
    scl_spinlock_init(&g->lock);
    return SCL_OK;
}

void scl_cgraph_destroy(scl_allocator_t *alloc, scl_concurrent_graph_t *g)
{
    if (!g) return;
    scl_spinlock_lock(&g->lock);
    for (size_t i = 0; i < g->vertex_count; i++) {
        scl_concurrent_adj_node_t *cur = g->adj[i];
        while (cur) {
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
    if (!g) return SCL_ERR_NULL_PTR;
    if (from >= g->vertex_count || to >= g->vertex_count) return SCL_ERR_INVALID_INDEX;
    scl_concurrent_adj_node_t *node = scl_alloc(alloc, sizeof(scl_concurrent_adj_node_t), alignof(max_align_t));
    if (!node) return SCL_ERR_OUT_OF_MEMORY;
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
    if (!g) return SCL_ERR_NULL_PTR;
    if (from >= g->vertex_count) return SCL_ERR_INVALID_INDEX;
    scl_spinlock_lock(&g->lock);
    scl_concurrent_adj_node_t **prev = &g->adj[from];
    scl_concurrent_adj_node_t *cur = g->adj[from];
    while (cur) {
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
    if (!g || from >= g->vertex_count) return false;
    scl_spinlock_lock((scl_spinlock_t *)&g->lock);
    scl_concurrent_adj_node_t *cur = g->adj[from];
    while (cur) {
        if (cur->to == to) { scl_spinlock_unlock((scl_spinlock_t *)&g->lock); return true; }
        cur = cur->next;
    }
    scl_spinlock_unlock((scl_spinlock_t *)&g->lock);
    return false;
}
