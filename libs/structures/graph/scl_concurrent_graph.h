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

/* Thread-safe graph: a flat-array graph core guarded by a spinlock. */

#ifndef SCL_CONCURRENT_GRAPH_H
#define SCL_CONCURRENT_GRAPH_H

#include "scl_concurrent_common.h"
#include "scl_graph.h"

#include "scl_stdbool.h"
#include "scl_stdint.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

/*
 * The concurrent graph wraps a plain scl_graph_t (the same two-array
 * representation) behind a spinlock, so it shares the entire implementation
 * and every graph search algorithm — no duplicated data structure or
 * algorithms.
 */
typedef struct {
  scl_graph_t core;
  scl_spinlock_t lock SCL_CACHE_ALIGNED;
} scl_concurrent_graph_t;

scl_error_t scl_cgraph_init(scl_allocator_t *SCL_RESTRICT alloc,
                            scl_concurrent_graph_t *SCL_RESTRICT g,
                            size_t vertex_count) SCL_WARN_UNUSED;
scl_error_t scl_cgraph_init_ex(scl_allocator_t *SCL_RESTRICT alloc,
                               scl_concurrent_graph_t *SCL_RESTRICT g,
                               size_t vertex_count,
                               size_t shard_len) SCL_WARN_UNUSED;
void scl_cgraph_destroy(scl_allocator_t *SCL_RESTRICT alloc,
                        scl_concurrent_graph_t *SCL_RESTRICT g);
scl_error_t scl_cgraph_add_edge(scl_allocator_t *SCL_RESTRICT alloc,
                                scl_concurrent_graph_t *SCL_RESTRICT g,
                                size_t from, size_t to,
                                int weight) SCL_WARN_UNUSED;
scl_error_t scl_cgraph_remove_edge(scl_allocator_t *SCL_RESTRICT alloc,
                                   scl_concurrent_graph_t *SCL_RESTRICT g,
                                   size_t from, size_t to) SCL_WARN_UNUSED;
bool scl_cgraph_has_edge(const scl_concurrent_graph_t *SCL_RESTRICT g,
                         size_t from, size_t to);

/* ── Search over the concurrent graph ────────────────────────────────────────
 * Each wrapper takes the lock, runs the corresponding scl_search_* on the core,
 * and releases it — so every graph search works on the concurrent graph with a
 * consistent snapshot. (Floyd-Warshall and A* don't operate on this graph type:
 * one takes an edge list, the other a grid.) For finer control, lock the graph
 * yourself and call scl_search_* on &g->core directly. */
scl_error_t scl_cgraph_dijkstra(scl_allocator_t *alloc,
                                scl_concurrent_graph_t *g, int start,
                                int64_t *dist, int *prev) SCL_WARN_UNUSED;
scl_error_t scl_cgraph_bellman_ford(scl_concurrent_graph_t *g, int start,
                                    int64_t *dist, int *prev) SCL_WARN_UNUSED;
scl_error_t scl_cgraph_bfs(scl_allocator_t *alloc, scl_concurrent_graph_t *g,
                           int start, bool *visited) SCL_WARN_UNUSED;
scl_error_t scl_cgraph_dfs(scl_allocator_t *alloc, scl_concurrent_graph_t *g,
                           int start, bool *visited) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif