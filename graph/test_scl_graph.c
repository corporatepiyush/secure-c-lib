#include "scl_graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static size_t g_visited[64];
static size_t g_count;

static void test_visit(size_t v, void *ctx)
{
    (void)ctx;
    g_visited[g_count++] = v;
}

static void test_add_has_remove(void)
{
    TEST("add/has/remove edge");
    scl_graph_t g;
    scl_graph_init(&g, 5);
    scl_graph_add_edge(&g, 0, 1, 1);
    scl_graph_add_edge(&g, 0, 2, 1);
    scl_graph_add_edge(&g, 1, 2, 1);
    assert(scl_graph_has_edge(&g, 0, 1));
    assert(!scl_graph_has_edge(&g, 1, 0));
    assert(scl_graph_edge_count(&g) == 3);
    scl_graph_remove_edge(&g, 0, 1);
    assert(!scl_graph_has_edge(&g, 0, 1));
    scl_graph_destroy(&g);
    PASS();
}

static void test_bounds(void)
{
    TEST("bounds checks");
    scl_graph_t g;
    scl_graph_init(&g, 3);
    assert(scl_graph_add_edge(&g, 5, 0, 1) == SCL_ERR_INVALID_INDEX);
    assert(scl_graph_has_edge(&g, 5, 0) == false);
    scl_graph_destroy(&g);
    PASS();
}

static void test_dfs_bfs(void)
{
    TEST("DFS and BFS");
    scl_graph_t g;
    scl_graph_init(&g, 4);
    scl_graph_add_edge(&g, 0, 1, 1);
    scl_graph_add_edge(&g, 0, 2, 1);
    scl_graph_add_edge(&g, 1, 3, 1);
    g_count = 0;
    assert(scl_graph_dfs(&g, 0, test_visit, NULL) == SCL_OK);
    g_count = 0;
    assert(scl_graph_bfs(&g, 0, test_visit, NULL) == SCL_OK);
    scl_graph_destroy(&g);
    PASS();
}

int main(void)
{
    printf("=== scl_graph tests ===\n");
    test_add_has_remove();
    test_bounds();
    test_dfs_bfs();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
