#ifndef SCL_CONCURRENT_GRAPH_H
#define SCL_CONCURRENT_GRAPH_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_concurrent_adj_node {
    size_t to;
    int weight;
    struct scl_concurrent_adj_node *next;
} scl_concurrent_adj_node_t;

typedef struct {
    scl_concurrent_adj_node_t **adj;
    size_t vertex_count;
    atomic_size_t edge_count;
    scl_spinlock_t lock;
} scl_concurrent_graph_t;

scl_error_t scl_cgraph_init(scl_allocator_t *alloc, scl_concurrent_graph_t *g, size_t vertex_count) SCL_WARN_UNUSED;
void        scl_cgraph_destroy(scl_allocator_t *alloc, scl_concurrent_graph_t *g);
scl_error_t scl_cgraph_add_edge(scl_allocator_t *alloc, scl_concurrent_graph_t *g, size_t from, size_t to, int weight) SCL_WARN_UNUSED;
scl_error_t scl_cgraph_remove_edge(scl_allocator_t *alloc, scl_concurrent_graph_t *g, size_t from, size_t to) SCL_WARN_UNUSED;
bool        scl_cgraph_has_edge(const scl_concurrent_graph_t *g, size_t from, size_t to);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
