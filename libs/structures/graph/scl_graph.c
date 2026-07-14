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

/* Graph data structure. All nodes and all edges are kept in two flat
 * scl_array containers; adjacency is an index chain threaded through the
 * edge array. */

#include "scl_graph.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_graph_init_ex(scl_allocator_t *alloc, scl_graph_t *g,
                              size_t vertex_count, size_t shard_len) {
  if (scl_unlikely(!g))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(vertex_count == 0))
    return SCL_ERR_INVALID_ARG;
  if (shard_len == 0)
    shard_len = SCL_GRAPH_DEFAULT_SHARD_LEN;

  g->vertex_count = vertex_count;
  g->edge_count = 0;
  g->free_head = SCL_GRAPH_NIL;

  scl_error_t err =
      scl_array_init(alloc, &g->nodes, sizeof(scl_graph_node_t), shard_len);
  if (err != SCL_OK) {
    g->vertex_count = 0;
    return err;
  }
  err = scl_array_init(alloc, &g->edges, sizeof(scl_graph_edge_t), shard_len);
  if (err != SCL_OK) {
    scl_array_destroy(alloc, &g->nodes);
    g->vertex_count = 0;
    return err;
  }

  /* Materialise one node record per vertex, each with an empty edge chain. */
  scl_graph_node_t empty = {SCL_GRAPH_NIL};
  for (size_t i = 0; i < vertex_count; i++) {
    if (scl_array_push(alloc, &g->nodes, &empty) != SCL_OK) {
      scl_array_destroy(alloc, &g->nodes);
      scl_array_destroy(alloc, &g->edges);
      return SCL_ERR_OUT_OF_MEMORY;
    }
  }

  return SCL_OK;
}

scl_error_t scl_graph_init(scl_allocator_t *alloc, scl_graph_t *g,
                           size_t vertex_count) {
  return scl_graph_init_ex(alloc, g, vertex_count, SCL_GRAPH_DEFAULT_SHARD_LEN);
}

void scl_graph_destroy(scl_allocator_t *alloc, scl_graph_t *g) {
  if (scl_unlikely(!g))
    return;
  scl_array_destroy(alloc, &g->nodes);
  scl_array_destroy(alloc, &g->edges);
  g->vertex_count = 0;
  g->edge_count = 0;
  g->free_head = SCL_GRAPH_NIL;
}

scl_error_t scl_graph_add_edge(scl_allocator_t *alloc, scl_graph_t *g,
                               size_t from, size_t to, int weight) {
  (void)alloc;
  if (scl_unlikely(!g))
    return SCL_ERR_NULL_PTR;
  if (from >= g->vertex_count || to >= g->vertex_count)
    return SCL_ERR_INVALID_INDEX;

  scl_graph_node_t *src =
      (scl_graph_node_t *)(g->nodes.data + from * sizeof(scl_graph_node_t));
  size_t idx;

  if (g->free_head != SCL_GRAPH_NIL) {
    /* Reuse a slot freed by a previous remove (no growth). */
    idx = g->free_head;
    scl_graph_edge_t *e =
        (scl_graph_edge_t *)(g->edges.data + idx * sizeof(scl_graph_edge_t));
    g->free_head = e->next; /* pop the free list */
    e->to = to;
    e->weight = weight;
    e->next = src->head;
  } else {
    scl_graph_edge_t e = {to, weight, src->head};
    scl_error_t err = scl_array_push(alloc, &g->edges, &e);
    if (err != SCL_OK)
      return err;
    idx = g->edges.count - 1;
  }

  src->head = idx; /* prepend to vertex's chain */
  g->edge_count++;
  return SCL_OK;
}

scl_error_t scl_graph_remove_edge(scl_allocator_t *alloc, scl_graph_t *g,
                                  size_t from, size_t to) {
  (void)alloc;
  if (scl_unlikely(!g))
    return SCL_ERR_NULL_PTR;
  if (from >= g->vertex_count || to >= g->vertex_count)
    return SCL_ERR_INVALID_INDEX;

  scl_graph_node_t *src =
      (scl_graph_node_t *)(g->nodes.data + from * sizeof(scl_graph_node_t));
  size_t *link = &src->head; /* slot holding the index to update */
  size_t e = src->head;
  while (e != SCL_GRAPH_NIL) {
    scl_graph_edge_t *ed =
        (scl_graph_edge_t *)(g->edges.data + e * sizeof(scl_graph_edge_t));
    if (ed->to == to) {
      *link = ed->next;        /* unlink from the adjacency chain */
      ed->next = g->free_head; /* recycle the slot via the free list */
      g->free_head = e;
      g->edge_count--;
      return SCL_OK;
    }
    link = &ed->next;
    e = ed->next;
  }
  return SCL_ERR_NOT_FOUND;
}

bool scl_graph_has_edge(const scl_graph_t *g, size_t from, size_t to) {
  if (!g || from >= g->vertex_count || to >= g->vertex_count)
    return false;
  for (size_t e = scl_graph_adj_head(g, from); e != SCL_GRAPH_NIL;) {
    const scl_graph_edge_t *ed = scl_graph_edge(g, e);
    if (ed->to == to)
      return true;
    e = ed->next;
  }
  return false;
}

size_t scl_graph_vertex_count(const scl_graph_t *g) {
  return g ? g->vertex_count : 0;
}
size_t scl_graph_edge_count(const scl_graph_t *g) {
  return g ? g->edge_count : 0;
}

scl_error_t scl_graph_dfs(scl_allocator_t *alloc, const scl_graph_t *g,
                          size_t start, void (*visit)(size_t, void *),
                          void *SCL_RESTRICT ctx) {
  if (scl_unlikely(!g || !visit))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(start >= g->vertex_count))
    return SCL_ERR_INVALID_INDEX;

  bool *visited =
      scl_calloc(alloc, g->vertex_count, sizeof(bool), alignof(max_align_t));
  if (scl_unlikely(!visited))
    return SCL_ERR_OUT_OF_MEMORY;

  size_t stack_sz;
  if (scl_mul_overflow(g->vertex_count, sizeof(size_t), &stack_sz)) {
    scl_free(alloc, visited);
    return SCL_ERR_SIZE_OVERFLOW;
  }
  size_t *stack = scl_alloc(alloc, stack_sz, alignof(max_align_t));
  if (!stack) {
    scl_free(alloc, visited);
    return SCL_ERR_OUT_OF_MEMORY;
  }

  size_t sp = 0;
  visited[start] = true;
  stack[sp++] = start;

  while (sp > 0) {
    size_t v = stack[--sp];
    visit(v, ctx);
    for (size_t e = scl_graph_adj_head(g, v); e != SCL_GRAPH_NIL;) {
      const scl_graph_edge_t *ed = scl_graph_edge(g, e);
      if (!visited[ed->to]) {
        visited[ed->to] = true;
        stack[sp++] = ed->to;
      }
      e = ed->next;
    }
  }

  scl_free(alloc, stack);
  scl_free(alloc, visited);
  return SCL_OK;
}

scl_error_t scl_graph_bfs(scl_allocator_t *alloc, const scl_graph_t *g,
                          size_t start, void (*visit)(size_t, void *),
                          void *SCL_RESTRICT ctx) {
  if (scl_unlikely(!g || !visit))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(start >= g->vertex_count))
    return SCL_ERR_INVALID_INDEX;

  bool *visited =
      scl_calloc(alloc, g->vertex_count, sizeof(bool), alignof(max_align_t));
  if (scl_unlikely(!visited))
    return SCL_ERR_OUT_OF_MEMORY;

  size_t queue_sz;
  if (scl_mul_overflow(g->vertex_count, sizeof(size_t), &queue_sz)) {
    scl_free(alloc, visited);
    return SCL_ERR_SIZE_OVERFLOW;
  }
  size_t *queue = scl_alloc(alloc, queue_sz, alignof(max_align_t));
  if (!queue) {
    scl_free(alloc, visited);
    return SCL_ERR_OUT_OF_MEMORY;
  }

  size_t qh = 0, qt = 0;
  visited[start] = true;
  queue[qt++] = start;

  while (qh < qt) {
    size_t v = queue[qh++];
    visit(v, ctx);
    for (size_t e = scl_graph_adj_head(g, v); e != SCL_GRAPH_NIL;) {
      const scl_graph_edge_t *ed = scl_graph_edge(g, e);
      if (!visited[ed->to]) {
        visited[ed->to] = true;
        queue[qt++] = ed->to;
      }
      e = ed->next;
    }
  }

  scl_free(alloc, queue);
  scl_free(alloc, visited);
  return SCL_OK;
}