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

/* Graph data structure. All nodes and all edges are kept in two sharded arrays;
 * adjacency is an index chain threaded through the edge array. */

#ifndef SCL_GRAPH_H
#define SCL_GRAPH_H

#include "scl_common.h"
#include "scl_sharded_array.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

/* Default shard length (elements per shard) for the node and edge arrays. */
#define SCL_GRAPH_DEFAULT_SHARD_LEN 256u

/* Initialise a graph with the default shard length. */
scl_error_t scl_graph_init(scl_allocator_t *SCL_RESTRICT alloc, scl_graph_t *SCL_RESTRICT g, size_t vertex_count) SCL_WARN_UNUSED;

/*
 * Like scl_graph_init, but the caller chooses the shard length of the
 * underlying node and edge sharded arrays. A shard holds `shard_len` records
 * (a node is sizeof(scl_graph_node_t) bytes, an edge sizeof(scl_graph_edge_t));
 * size shard_len so a shard is a convenient multiple of the cache line.
 * shard_len == 0 uses SCL_GRAPH_DEFAULT_SHARD_LEN.
 */
scl_error_t scl_graph_init_ex(scl_allocator_t *SCL_RESTRICT alloc, scl_graph_t *SCL_RESTRICT g, size_t vertex_count, size_t shard_len) SCL_WARN_UNUSED;

void        scl_graph_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_graph_t *SCL_RESTRICT g);
scl_error_t scl_graph_add_edge(scl_allocator_t *SCL_RESTRICT alloc, scl_graph_t *SCL_RESTRICT g, size_t from, size_t to, int weight) SCL_WARN_UNUSED;
scl_error_t scl_graph_remove_edge(scl_allocator_t *SCL_RESTRICT alloc, scl_graph_t *SCL_RESTRICT g, size_t from, size_t to) SCL_WARN_UNUSED;
bool        scl_graph_has_edge(const scl_graph_t *SCL_RESTRICT g, size_t from, size_t to);
SCL_PURE size_t      scl_graph_vertex_count(const scl_graph_t *SCL_RESTRICT g);
SCL_PURE size_t      scl_graph_edge_count(const scl_graph_t *SCL_RESTRICT g);

/* ── Adjacency traversal ─────────────────────────────────────────────────────
 * Index of vertex v's first outgoing edge (or SCL_GRAPH_NIL), and the edge
 * record at index e. Iterate:
 *
 *     for (size_t e = scl_graph_adj_head(g, v); e != SCL_GRAPH_NIL; ) {
 *         const scl_graph_edge_t *ed = scl_graph_edge(g, e);
 *         use ed->to, ed->weight;
 *         e = ed->next;
 *     }
 */
static inline size_t scl_graph_adj_head(const scl_graph_t *g, size_t v)
{
    const scl_graph_node_t *n = (const scl_graph_node_t *)scl_sharded_array_get(&g->nodes, v);
    return n ? n->head : SCL_GRAPH_NIL;
}

static inline const scl_graph_edge_t *scl_graph_edge(const scl_graph_t *g, size_t e)
{
    return (const scl_graph_edge_t *)scl_sharded_array_get(&g->edges, e);
}

scl_error_t scl_graph_dfs(scl_allocator_t *alloc, const scl_graph_t *g, size_t start,
                          void (*visit)(size_t vertex, void *SCL_RESTRICT ctx),
                          void *ctx) SCL_WARN_UNUSED;
scl_error_t scl_graph_bfs(scl_allocator_t *alloc, const scl_graph_t *g, size_t start,
                          void (*visit)(size_t vertex, void *SCL_RESTRICT ctx),
                          void *ctx) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
