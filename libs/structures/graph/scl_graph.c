#include "scl_graph.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_graph_init(scl_allocator_t *alloc, scl_graph_t *g, size_t vertex_count)
{
    if (scl_unlikely(!g)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(vertex_count == 0)) return SCL_ERR_INVALID_ARG;

    g->adj = scl_calloc(alloc, vertex_count, sizeof(scl_graph_edge_t *), alignof(max_align_t));
    if (scl_unlikely(!g->adj)) return SCL_ERR_OUT_OF_MEMORY;

    g->vertex_count = vertex_count;
    g->edge_count = 0;
    return SCL_OK;
}

void scl_graph_destroy(scl_allocator_t *alloc, scl_graph_t *g)
{
    if (scl_unlikely(!g)) return;
    for (size_t i = 0; i < g->vertex_count; i++) {
        scl_graph_edge_t *e = g->adj[i];
        while (scl_likely(e)) {
            scl_graph_edge_t *next = e->next;
            scl_free(alloc, e);
            e = next;
        }
    }
    scl_free(alloc, g->adj);
    g->adj = NULL;
    g->vertex_count = 0;
    g->edge_count = 0;
}

scl_error_t scl_graph_add_edge(scl_allocator_t *alloc, scl_graph_t *g, size_t from, size_t to, int weight)
{
    if (scl_unlikely(!g)) return SCL_ERR_NULL_PTR;
    if (from >= g->vertex_count || to >= g->vertex_count)
        return SCL_ERR_INVALID_INDEX;

    scl_graph_edge_t *e = scl_alloc(alloc, sizeof(scl_graph_edge_t), alignof(max_align_t));
    if (scl_unlikely(!e)) return SCL_ERR_OUT_OF_MEMORY;
    e->to = to;
    e->weight = weight;
    e->next = g->adj[from];
    g->adj[from] = e;
    g->edge_count++;
    return SCL_OK;
}

scl_error_t scl_graph_remove_edge(scl_allocator_t *alloc, scl_graph_t *g, size_t from, size_t to)
{
    if (scl_unlikely(!g)) return SCL_ERR_NULL_PTR;
    if (from >= g->vertex_count || to >= g->vertex_count)
        return SCL_ERR_INVALID_INDEX;

    scl_graph_edge_t **prev = &g->adj[from];
    scl_graph_edge_t *e = g->adj[from];
    while (scl_likely(e)) {
        if (e->to == to) {
            *prev = e->next;
            scl_free(alloc, e);
            g->edge_count--;
            return SCL_OK;
        }
        prev = &e->next;
        e = e->next;
    }
    return SCL_ERR_NOT_FOUND;
}

bool scl_graph_has_edge(const scl_graph_t *g, size_t from, size_t to)
{
    if (!g || from >= g->vertex_count || to >= g->vertex_count)
        return false;
    scl_graph_edge_t *e = g->adj[from];
    while (scl_likely(e)) {
        if (e->to == to) return true;
        e = e->next;
    }
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

    size_t sp = 0;
    stack[sp++] = start;

    while (sp > 0) {
        size_t v = stack[--sp];
        if (visited[v]) continue;
        visited[v] = true;
        visit(v, ctx);
        scl_graph_edge_t *e = g->adj[v];
        while (scl_likely(e)) {
            if (!visited[e->to])
                stack[sp++] = e->to;
            e = e->next;
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
        scl_graph_edge_t *e = g->adj[v];
        while (scl_likely(e)) {
            if (!visited[e->to]) {
                visited[e->to] = true;
                queue[qt++] = e->to;
            }
            e = e->next;
        }
    }

    scl_free(alloc, queue);
    scl_free(alloc, visited);
    return SCL_OK;
}
