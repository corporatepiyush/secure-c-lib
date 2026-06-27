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

typedef scl_adj_node_t scl_graph_edge_t;

scl_error_t scl_graph_init(scl_allocator_t *SCL_RESTRICT alloc, scl_graph_t *SCL_RESTRICT g, size_t vertex_count) SCL_WARN_UNUSED;
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
