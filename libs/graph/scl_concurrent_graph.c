#include "scl_concurrent_graph.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static inline void spin_lock(atomic_flag *lock)
{
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {
        scl_cpu_pause();
    }
}

static inline void spin_unlock(atomic_flag *lock)
{
    atomic_flag_clear_explicit(lock, memory_order_release);
}

scl_error_t scl_cgraph_init(scl_allocator_t *alloc, scl_concurrent_graph_t *g, size_t vertex_count)
{
    if (!g) return SCL_ERR_NULL_PTR;
    if (vertex_count == 0) return SCL_ERR_INVALID_ARG;
    g->adj = scl_calloc(alloc, vertex_count, sizeof(scl_concurrent_adj_node_t *), alignof(max_align_t));
    if (!g->adj) return SCL_ERR_OUT_OF_MEMORY;
    g->vertex_count = vertex_count;
    atomic_init(&g->edge_count, 0);
    atomic_flag_clear(&g->lock);
    return SCL_OK;
}

void scl_cgraph_destroy(scl_allocator_t *alloc, scl_concurrent_graph_t *g)
{
    if (!g) return;
    spin_lock(&g->lock);
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
    spin_unlock(&g->lock);
}

scl_error_t scl_cgraph_add_edge(scl_allocator_t *alloc, scl_concurrent_graph_t *g, size_t from, size_t to, int weight)
{
    if (!g) return SCL_ERR_NULL_PTR;
    if (from >= g->vertex_count || to >= g->vertex_count) return SCL_ERR_INVALID_INDEX;
    scl_concurrent_adj_node_t *node = scl_alloc(alloc, sizeof(scl_concurrent_adj_node_t), alignof(max_align_t));
    if (!node) return SCL_ERR_OUT_OF_MEMORY;
    node->to = to;
    node->weight = weight;
    spin_lock(&g->lock);
    node->next = g->adj[from];
    g->adj[from] = node;
    atomic_fetch_add_explicit(&g->edge_count, 1, memory_order_relaxed);
    spin_unlock(&g->lock);
    return SCL_OK;
}

scl_error_t scl_cgraph_remove_edge(scl_allocator_t *alloc, scl_concurrent_graph_t *g, size_t from, size_t to)
{
    if (!g) return SCL_ERR_NULL_PTR;
    if (from >= g->vertex_count) return SCL_ERR_INVALID_INDEX;
    spin_lock(&g->lock);
    scl_concurrent_adj_node_t **prev = &g->adj[from];
    scl_concurrent_adj_node_t *cur = g->adj[from];
    while (cur) {
        if (cur->to == to) {
            *prev = cur->next;
            scl_free(alloc, cur);
            atomic_fetch_sub_explicit(&g->edge_count, 1, memory_order_relaxed);
            spin_unlock(&g->lock);
            return SCL_OK;
        }
        prev = &cur->next;
        cur = cur->next;
    }
    spin_unlock(&g->lock);
    return SCL_ERR_NOT_FOUND;
}

bool scl_cgraph_has_edge(const scl_concurrent_graph_t *g, size_t from, size_t to)
{
    if (!g || from >= g->vertex_count) return false;
    spin_lock((atomic_flag *)&g->lock);
    scl_concurrent_adj_node_t *cur = g->adj[from];
    while (cur) {
        if (cur->to == to) { spin_unlock((atomic_flag *)&g->lock); return true; }
        cur = cur->next;
    }
    spin_unlock((atomic_flag *)&g->lock);
    return false;
}
