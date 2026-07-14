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

/* Tests for the sharded-array graph data structure.
 *
 * Covers:
 *   1. init / destroy
 *   2. vertex count, edge count
 *   3. add edge (forward star prepend) and has_edge
 *   4. remove edge (free-list recycling)
 *   5. adjacency walk
 */

#include "scl_graph.h"
#include "scl_test.h"

/* ── 1. init / destroy ──────────────────────────────────────── */
static void test_graph_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Graph: init and destroy");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_graph_t g;
  scl_error_t err = scl_graph_init(alloc, &g, 5);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_SZ(tr, scl_graph_vertex_count(&g), 5);
  SCL_EXPECT_EQ_SZ(tr, scl_graph_edge_count(&g), 0);

  scl_graph_destroy(alloc, &g);
  TEST_TRACE_END();
}

/* ── 2. add edge ───────────────────────────────────────────── */
static void test_graph_add_edge(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Graph: add edge");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_graph_t g;
  scl_graph_init(alloc, &g, 7);

  scl_error_t err = scl_graph_add_edge(alloc, &g, 0, 1, 10);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_TRUE(tr, scl_graph_has_edge(&g, 0, 1));
  SCL_EXPECT_FALSE(tr, scl_graph_has_edge(&g, 1, 0));
  SCL_EXPECT_EQ_SZ(tr, scl_graph_edge_count(&g), 1);

  scl_graph_destroy(alloc, &g);
  TEST_TRACE_END();
}

/* ── 3. remove edge ────────────────────────────────────────── */
static void test_graph_remove_edge(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Graph: remove edge");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_graph_t g;
  scl_graph_init(alloc, &g, 7);

  scl_graph_add_edge(alloc, &g, 0, 1, 10);
  scl_error_t err = scl_graph_remove_edge(alloc, &g, 0, 1);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_FALSE(tr, scl_graph_has_edge(&g, 0, 1));
  SCL_EXPECT_EQ_SZ(tr, scl_graph_edge_count(&g), 0);

  scl_graph_destroy(alloc, &g);
  TEST_TRACE_END();
}

/* ── 4. adjacency walk ─────────────────────────────────────── */
static void test_graph_adj_walk(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Graph: adjacency walk");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_graph_t g;
  scl_graph_init(alloc, &g, 5);

  scl_graph_add_edge(alloc, &g, 0, 1, 10);
  scl_graph_add_edge(alloc, &g, 0, 2, 20);
  scl_graph_add_edge(alloc, &g, 0, 3, 30);

  /* Walk adjacency list of vertex 0 and verify destinations. */
  int seen = 0;
  for (size_t e = scl_graph_adj_head(&g, 0); e != SCL_GRAPH_NIL;) {
    const scl_graph_edge_t *ed = scl_graph_edge(&g, e);
    SCL_EXPECT_TRUE(tr, ed->to >= 1 && ed->to <= 3);
    seen |= (1 << ed->to);
    e = ed->next;
  }
  SCL_EXPECT_EQ_I(tr, seen, 0xE); /* bits 1,2,3 set (0b1110) */

  scl_graph_destroy(alloc, &g);
  TEST_TRACE_END();
}

/* ── 5. multiple edges and free-list recycle ───────────────── */
static void test_graph_multi_edge(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Graph: multi-edge and free-list recycle");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_graph_t g;
  scl_graph_init(alloc, &g, 5);

  /* Add 4 out-edges from vertex 0. */
  for (int i = 1; i <= 4; i++)
    scl_graph_add_edge(alloc, &g, 0, (size_t)i, i);
  SCL_EXPECT_EQ_SZ(tr, scl_graph_edge_count(&g), 4);

  /* Remove 2 of them. */
  scl_graph_remove_edge(alloc, &g, 0, 2);
  scl_graph_remove_edge(alloc, &g, 0, 3);
  SCL_EXPECT_FALSE(tr, scl_graph_has_edge(&g, 0, 2));
  SCL_EXPECT_FALSE(tr, scl_graph_has_edge(&g, 0, 3));
  SCL_EXPECT_EQ_SZ(tr, scl_graph_edge_count(&g), 2);

  /* Re-add — should reuse freed slots, edge count stays 3. */
  scl_graph_add_edge(alloc, &g, 0, 2, 20);
  SCL_EXPECT_EQ_SZ(tr, scl_graph_edge_count(&g), 3);
  SCL_EXPECT_TRUE(tr, scl_graph_has_edge(&g, 0, 2));

  scl_graph_destroy(alloc, &g);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_graph_init_destroy(&tr);
  test_graph_add_edge(&tr);
  test_graph_remove_edge(&tr);
  test_graph_adj_walk(&tr);
  test_graph_multi_edge(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}