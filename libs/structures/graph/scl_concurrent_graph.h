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
    scl_spinlock_t lock SCL_CACHE_ALIGNED;
} scl_concurrent_graph_t;

scl_error_t scl_cgraph_init(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_graph_t *SCL_RESTRICT g, size_t vertex_count) SCL_WARN_UNUSED;
void        scl_cgraph_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_graph_t *SCL_RESTRICT g);
scl_error_t scl_cgraph_add_edge(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_graph_t *SCL_RESTRICT g, size_t from, size_t to, int weight) SCL_WARN_UNUSED;
scl_error_t scl_cgraph_remove_edge(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_graph_t *SCL_RESTRICT g, size_t from, size_t to) SCL_WARN_UNUSED;
bool        scl_cgraph_has_edge(const scl_concurrent_graph_t *SCL_RESTRICT g, size_t from, size_t to);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
