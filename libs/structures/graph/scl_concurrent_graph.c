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

/* Thread-safe graph: a sharded-array graph core guarded by a spinlock. */

#include "scl_concurrent_graph.h"
#include "scl_dijkstra.h"
#include "scl_bellman_ford.h"
#include "scl_breadth_first.h"
#include "scl_depth_first.h"

scl_error_t scl_cgraph_init_ex(scl_allocator_t *alloc, scl_concurrent_graph_t *g, size_t vertex_count, size_t shard_len)
{
    if (scl_unlikely(!g)) return SCL_ERR_NULL_PTR;
    scl_error_t err = scl_graph_init_ex(alloc, &g->core, vertex_count, shard_len);
    if (err != SCL_OK) return err;
    scl_spinlock_init(&g->lock);
    return SCL_OK;
}

scl_error_t scl_cgraph_init(scl_allocator_t *alloc, scl_concurrent_graph_t *g, size_t vertex_count)
{
    return scl_cgraph_init_ex(alloc, g, vertex_count, SCL_GRAPH_DEFAULT_SHARD_LEN);
}

void scl_cgraph_destroy(scl_allocator_t *alloc, scl_concurrent_graph_t *g)
{
    if (scl_unlikely(!g)) return;
    scl_spinlock_lock(&g->lock);
    scl_graph_destroy(alloc, &g->core);
    scl_spinlock_unlock(&g->lock);
}

scl_error_t scl_cgraph_add_edge(scl_allocator_t *alloc, scl_concurrent_graph_t *g, size_t from, size_t to, int weight)
{
    if (scl_unlikely(!g)) return SCL_ERR_NULL_PTR;
    scl_spinlock_lock(&g->lock);
    scl_error_t r = scl_graph_add_edge(alloc, &g->core, from, to, weight);
    scl_spinlock_unlock(&g->lock);
    return r;
}

scl_error_t scl_cgraph_remove_edge(scl_allocator_t *alloc, scl_concurrent_graph_t *g, size_t from, size_t to)
{
    if (scl_unlikely(!g)) return SCL_ERR_NULL_PTR;
    scl_spinlock_lock(&g->lock);
    scl_error_t r = scl_graph_remove_edge(alloc, &g->core, from, to);
    scl_spinlock_unlock(&g->lock);
    return r;
}

bool scl_cgraph_has_edge(const scl_concurrent_graph_t *g, size_t from, size_t to)
{
    if (scl_unlikely(!g)) return false;
    scl_spinlock_t *lock = (scl_spinlock_t *)&g->lock;   /* logical, not bitwise, const */
    scl_spinlock_lock(lock);
    bool r = scl_graph_has_edge(&g->core, from, to);
    scl_spinlock_unlock(lock);
    return r;
}

/* ── Search wrappers: lock, run the core algorithm, unlock. ─────────────────── */

scl_error_t scl_cgraph_dijkstra(scl_allocator_t *alloc, scl_concurrent_graph_t *g, int start, int64_t *dist, int *prev)
{
    if (scl_unlikely(!g)) return SCL_ERR_NULL_PTR;
    scl_spinlock_lock(&g->lock);
    scl_error_t r = scl_search_dijkstra(alloc, &g->core, start, dist, prev);
    scl_spinlock_unlock(&g->lock);
    return r;
}

scl_error_t scl_cgraph_bellman_ford(scl_concurrent_graph_t *g, int start, int64_t *dist, int *prev)
{
    if (scl_unlikely(!g)) return SCL_ERR_NULL_PTR;
    scl_spinlock_lock(&g->lock);
    scl_error_t r = scl_search_bellman_ford(&g->core, start, dist, prev);
    scl_spinlock_unlock(&g->lock);
    return r;
}

scl_error_t scl_cgraph_bfs(scl_allocator_t *alloc, scl_concurrent_graph_t *g, int start, bool *visited)
{
    if (scl_unlikely(!g)) return SCL_ERR_NULL_PTR;
    scl_spinlock_lock(&g->lock);
    scl_error_t r = scl_search_breadth_first_search(alloc, &g->core, start, visited);
    scl_spinlock_unlock(&g->lock);
    return r;
}

scl_error_t scl_cgraph_dfs(scl_allocator_t *alloc, scl_concurrent_graph_t *g, int start, bool *visited)
{
    if (scl_unlikely(!g)) return SCL_ERR_NULL_PTR;
    scl_spinlock_lock(&g->lock);
    scl_error_t r = scl_search_depth_first_search(alloc, &g->core, start, visited);
    scl_spinlock_unlock(&g->lock);
    return r;
}
