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

#ifndef SCL_GRAPH_H
#define SCL_GRAPH_H

#include "scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

/*
 * Default per-vertex shard capacity. scl_adj_entry_t is 16 bytes, so 4 entries
 * occupy exactly one 64-byte cache line — a sensible default for a vertex's
 * first edges.
 */
#define SCL_GRAPH_DEFAULT_SHARD_CAP 4u

/* Initialise a graph. Each vertex's edge shard is allocated lazily (on its
 * first edge) with SCL_GRAPH_DEFAULT_SHARD_CAP slots, then doubles. */
scl_error_t scl_graph_init(scl_allocator_t *SCL_RESTRICT alloc, scl_graph_t *SCL_RESTRICT g, size_t vertex_count) SCL_WARN_UNUSED;

/*
 * Like scl_graph_init, but the caller chooses the initial capacity of each
 * vertex's edge shard (in edges). Tune this to the target CPU's cache: a shard
 * of `shard_cap` edges occupies shard_cap * sizeof(scl_adj_entry_t) bytes, so
 * e.g. shard_cap = cache_line_size / sizeof(scl_adj_entry_t) keeps a vertex's
 * initial edge block to a single line. The shard is still allocated lazily (no
 * memory is reserved for vertices that get no edges) and grows by doubling.
 * shard_cap == 0 uses SCL_GRAPH_DEFAULT_SHARD_CAP.
 */
scl_error_t scl_graph_init_ex(scl_allocator_t *SCL_RESTRICT alloc, scl_graph_t *SCL_RESTRICT g, size_t vertex_count, size_t shard_cap) SCL_WARN_UNUSED;
void        scl_graph_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_graph_t *SCL_RESTRICT g);
scl_error_t scl_graph_add_edge(scl_allocator_t *SCL_RESTRICT alloc, scl_graph_t *SCL_RESTRICT g, size_t from, size_t to, int weight) SCL_WARN_UNUSED;
scl_error_t scl_graph_remove_edge(scl_allocator_t *SCL_RESTRICT alloc, scl_graph_t *SCL_RESTRICT g, size_t from, size_t to) SCL_WARN_UNUSED;
bool        scl_graph_has_edge(const scl_graph_t *SCL_RESTRICT g, size_t from, size_t to);
SCL_PURE size_t      scl_graph_vertex_count(const scl_graph_t *SCL_RESTRICT g);
SCL_PURE size_t      scl_graph_edge_count(const scl_graph_t *SCL_RESTRICT g);

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
