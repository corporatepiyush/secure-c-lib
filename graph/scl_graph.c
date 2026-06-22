#include "scl_graph.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_graph_init(scl_graph_t *g, size_t vertex_count)
{
    if (!g) return SCL_ERR_NULL_PTR;
    if (vertex_count == 0) return SCL_ERR_INVALID_ARG;

    g->adj = calloc(vertex_count, sizeof(scl_graph_edge_t *));
    if (!g->adj) return SCL_ERR_OUT_OF_MEMORY;

    g->vertex_count = vertex_count;
    g->edge_count = 0;
    return SCL_OK;
}

void scl_graph_destroy(scl_graph_t *g)
{
    if (!g) return;
    for (size_t i = 0; i < g->vertex_count; i++) {
        scl_graph_edge_t *e = g->adj[i];
        while (e) {
            scl_graph_edge_t *next = e->next;
            free(e);
            e = next;
        }
    }
    free(g->adj);
    g->adj = NULL;
    g->vertex_count = 0;
    g->edge_count = 0;
}

scl_error_t scl_graph_add_edge(scl_graph_t *g, size_t from, size_t to, int weight)
{
    if (!g) return SCL_ERR_NULL_PTR;
    if (from >= g->vertex_count || to >= g->vertex_count)
        return SCL_ERR_INVALID_INDEX;

    scl_graph_edge_t *e = malloc(sizeof(scl_graph_edge_t));
    if (!e) return SCL_ERR_OUT_OF_MEMORY;
    e->to = to;
    e->weight = weight;
    e->next = g->adj[from];
    g->adj[from] = e;
    g->edge_count++;
    return SCL_OK;
}

scl_error_t scl_graph_remove_edge(scl_graph_t *g, size_t from, size_t to)
{
    if (!g) return SCL_ERR_NULL_PTR;
    if (from >= g->vertex_count || to >= g->vertex_count)
        return SCL_ERR_INVALID_INDEX;

    scl_graph_edge_t **prev = &g->adj[from];
    scl_graph_edge_t *e = g->adj[from];
    while (e) {
        if (e->to == to) {
            *prev = e->next;
            free(e);
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
    while (e) {
        if (e->to == to) return true;
        e = e->next;
    }
    return false;
}

size_t scl_graph_vertex_count(const scl_graph_t *g) { return g ? g->vertex_count : 0; }
size_t scl_graph_edge_count(const scl_graph_t *g) { return g ? g->edge_count : 0; }

static bool scl_graph_dfs_visit(const scl_graph_t *g, size_t v, bool *restrict visited,
                                 void (*visit)(size_t, void *), void *ctx)
{
    visited[v] = true;
    visit(v, ctx);
    scl_graph_edge_t *e = g->adj[v];
    while (e) {
        if (!visited[e->to]) {
            if (!scl_graph_dfs_visit(g, e->to, visited, visit, ctx))
                return false;
        }
        e = e->next;
    }
    return true;
}

scl_error_t scl_graph_dfs(const scl_graph_t *restrict g, size_t start,
                          void (*visit)(size_t, void *), void *ctx)
{
    if (__builtin_expect(!g || !visit, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(start >= g->vertex_count, 0)) return SCL_ERR_INVALID_INDEX;

    bool *visited = calloc(g->vertex_count, sizeof(bool));
    if (!visited) return SCL_ERR_OUT_OF_MEMORY;

    scl_graph_dfs_visit(g, start, visited, visit, ctx);
    free(visited);
    return SCL_OK;
}

scl_error_t scl_graph_bfs(const scl_graph_t *restrict g, size_t start,
                          void (*visit)(size_t, void *), void *ctx)
{
    if (__builtin_expect(!g || !visit, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(start >= g->vertex_count, 0)) return SCL_ERR_INVALID_INDEX;

    bool *visited = calloc(g->vertex_count, sizeof(bool));
    if (!visited) return SCL_ERR_OUT_OF_MEMORY;

    size_t *queue = malloc(g->vertex_count * sizeof(size_t));
    if (!queue) { free(visited); return SCL_ERR_OUT_OF_MEMORY; }

    size_t qh = 0, qt = 0;
    visited[start] = true;
    queue[qt++] = start;

    while (qh < qt) {
        size_t v = queue[qh++];
        visit(v, ctx);
        scl_graph_edge_t *e = g->adj[v];
        while (e) {
            if (!visited[e->to]) {
                visited[e->to] = true;
                queue[qt++] = e->to;
            }
            e = e->next;
        }
    }

    free(queue);
    free(visited);
    return SCL_OK;
}
