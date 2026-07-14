/* Tests for the graph search algorithms over the sharded-array graph:
 * Dijkstra (binary heap + non-negative-weight guard), Bellman-Ford (negative
 * edges + negative-cycle detection), BFS/DFS reachability (including a dense
 * E>V graph that overflowed the old DFS stack), Floyd-Warshall, and A*.
 * Run under ASan. */
#include "scl_a_star.h"
#include "scl_bellman_ford.h"
#include "scl_breadth_first.h"
#include "scl_depth_first.h"
#include "scl_dijkstra.h"
#include "scl_floyd_warshall.h"
#include "scl_graph.h"
#include "scl_test.h"

#include <stdlib.h>
#include <string.h>

#define E(g, a, b, w) SCL_EXPECT_OK(tr, scl_graph_add_edge(al, g, a, b, w))

static void test_dijkstra(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Dijkstra: shortest paths + negative-weight rejection");
  scl_allocator_t *al = scl_allocator_default();
  scl_graph_t g;
  SCL_EXPECT_OK(tr, scl_graph_init(al, &g, 4));
  E(&g, 0, 1, 4);
  E(&g, 0, 2, 1);
  E(&g, 2, 1, 2);
  E(&g, 1, 3, 1);
  E(&g, 2, 3, 5);

  int64_t dist[4];
  int prev[4];
  SCL_EXPECT_OK(tr, scl_search_dijkstra(al, &g, 0, dist, prev));
  SCL_EXPECT_EQ_I(tr, dist[0], 0);
  SCL_EXPECT_EQ_I(tr, dist[1], 3); /* 0->2->1 */
  SCL_EXPECT_EQ_I(tr, dist[2], 1);
  SCL_EXPECT_EQ_I(tr, dist[3], 4); /* 0->2->1->3 */
  SCL_EXPECT_EQ_I(tr, prev[3], 1);
  scl_graph_destroy(al, &g);

  /* A negative weight must be rejected, not silently mis-handled. */
  scl_graph_t ng;
  SCL_EXPECT_OK(tr, scl_graph_init(al, &ng, 3));
  E(&ng, 0, 1, 2);
  E(&ng, 1, 2, -3);
  SCL_EXPECT_TRUE(tr, scl_search_dijkstra(al, &ng, 0, dist, prev) ==
                          SCL_ERR_INVALID_ARG);
  scl_graph_destroy(al, &ng);
  TEST_TRACE_END();
}

static void test_bellman_ford(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Bellman-Ford: negative edges + cycle detection");
  scl_allocator_t *al = scl_allocator_default();

  /* Negative edge, no cycle. */
  scl_graph_t g;
  SCL_EXPECT_OK(tr, scl_graph_init(al, &g, 4));
  E(&g, 0, 1, 4);
  E(&g, 0, 2, 5);
  E(&g, 1, 2, -3);
  E(&g, 2, 3, 2);
  int64_t dist[4];
  int prev[4];
  SCL_EXPECT_OK(tr, scl_search_bellman_ford(&g, 0, dist, prev));
  SCL_EXPECT_EQ_I(tr, dist[2], 1); /* 0->1->2 = 4-3 */
  SCL_EXPECT_EQ_I(tr, dist[3], 3); /* +2 */
  scl_graph_destroy(al, &g);

  /* Reachable negative cycle 1->2->1 (sum -2). */
  scl_graph_t c;
  SCL_EXPECT_OK(tr, scl_graph_init(al, &c, 3));
  E(&c, 0, 1, 1);
  E(&c, 1, 2, -1);
  E(&c, 2, 1, -1);
  SCL_EXPECT_TRUE(tr, scl_search_bellman_ford(&c, 0, dist, prev) ==
                          SCL_ERR_INVALID_STATE);
  scl_graph_destroy(al, &c);
  TEST_TRACE_END();
}

static void test_bfs_dfs(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("BFS/DFS: reachability incl. dense E>V graph");
  scl_allocator_t *al = scl_allocator_default();

  /* Dense graph: 8 vertices, every ordered pair (i<j) connected -> 28 edges,
   * far more than 8 vertices. The old DFS pushed once per in-edge into a
   * V-sized stack and overflowed; this must run clean under ASan. */
  size_t n = 8;
  scl_graph_t g;
  SCL_EXPECT_OK(tr, scl_graph_init(al, &g, n));
  for (size_t i = 0; i < n; i++)
    for (size_t j = 0; j < n; j++)
      if (i != j)
        SCL_EXPECT_OK(tr, scl_graph_add_edge(al, &g, i, j, 1));
  SCL_EXPECT_EQ_SZ(tr, scl_graph_edge_count(&g), n * (n - 1));

  bool *vis = (bool *)calloc(n, sizeof(bool));
  SCL_EXPECT_OK(tr, scl_search_depth_first_search(al, &g, 0, vis));
  int all = 1;
  for (size_t i = 0; i < n; i++)
    all &= vis[i];
  SCL_EXPECT_TRUE(tr, all); /* DFS reaches everything */

  memset(vis, 0, n * sizeof(bool));
  SCL_EXPECT_OK(tr, scl_search_breadth_first_search(al, &g, 0, vis));
  all = 1;
  for (size_t i = 0; i < n; i++)
    all &= vis[i];
  SCL_EXPECT_TRUE(tr, all);
  free(vis);
  scl_graph_destroy(al, &g);

  /* Disconnected: vertex 3 unreachable from 0. */
  scl_graph_t d;
  SCL_EXPECT_OK(tr, scl_graph_init(al, &d, 4));
  E(&d, 0, 1, 1);
  E(&d, 1, 2, 1);
  bool v2[4] = {false, false, false, false};
  SCL_EXPECT_OK(tr, scl_search_depth_first_search(al, &d, 0, v2));
  SCL_EXPECT_TRUE(tr, v2[0] && v2[1] && v2[2]);
  SCL_EXPECT_FALSE(tr, v2[3]);
  scl_graph_destroy(al, &d);
  TEST_TRACE_END();
}

static void test_floyd_warshall(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Floyd-Warshall: all-pairs shortest paths");
  int n = 4;
  scl_edge_t edges[] = {
      {0, 1, 3}, {0, 2, 8}, {1, 2, 2}, {2, 3, 1}, {1, 3, 7},
  };
  int64_t *dist = (int64_t *)malloc((size_t)n * n * sizeof(int64_t));
  SCL_EXPECT_NOT_NULL(tr, dist);
  if (!dist)
    return;
  SCL_EXPECT_OK(
      tr, scl_search_floyd_warshall(n, edges, SCL_ARRAY_SIZE(edges), dist));
  SCL_EXPECT_EQ_I(tr, dist[0 * n + 2], 5); /* 0->1->2 */
  SCL_EXPECT_EQ_I(tr, dist[0 * n + 3], 6); /* 0->1->2->3 */
  SCL_EXPECT_EQ_I(tr, dist[1 * n + 3], 3); /* 1->2->3 */
  SCL_EXPECT_EQ_I(tr, dist[0 * n + 0], 0);
  free(dist);
  TEST_TRACE_END();
}

static void test_a_star(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("A*: grid pathfinding + blocked goal");
  scl_allocator_t *al = scl_allocator_default();
  int w = 5, h = 5;
  /* 0 = free, 1 = wall. A vertical wall with a gap at the bottom row. */
  int rows[5][5] = {
      {0, 0, 0, 0, 0}, {0, 1, 1, 1, 0}, {0, 1, 0, 1, 0},
      {0, 1, 0, 1, 0}, {0, 0, 0, 0, 0},
  };
  int *grid[5];
  for (int i = 0; i < 5; i++)
    grid[i] = rows[i];

  int px[64], py[64];
  size_t plen = 0;
  SCL_EXPECT_OK(
      tr, scl_search_a_star(al, 0, 0, 2, 2, grid, w, h, px, py, &plen, 64));
  SCL_EXPECT_TRUE(tr, plen > 0);
  SCL_EXPECT_EQ_I(tr, px[0], 0);
  SCL_EXPECT_EQ_I(tr, py[0], 0); /* starts at start */
  SCL_EXPECT_EQ_I(tr, px[plen - 1], 2);
  SCL_EXPECT_EQ_I(tr, py[plen - 1], 2); /* ends at goal */

  /* Fully wall off a goal cell's neighbourhood -> unreachable. */
  int rows2[5][5] = {
      {0, 0, 1, 0, 0}, {0, 0, 1, 0, 0}, {1, 1, 1, 0, 0},
      {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},
  };
  int *grid2[5];
  for (int i = 0; i < 5; i++)
    grid2[i] = rows2[i];
  SCL_EXPECT_TRUE(tr, scl_search_a_star(al, 0, 0, 0, 4, grid2, w, h, px, py,
                                        &plen, 64) == SCL_ERR_NOT_FOUND);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);
  test_dijkstra(&tr);
  test_bellman_ford(&tr);
  test_bfs_dfs(&tr);
  test_floyd_warshall(&tr);
  test_a_star(&tr);
  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
