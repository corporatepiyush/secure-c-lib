#include "scl_graph.h"
#include "../../testlib/scl_test.h"

static size_t g_visited[64];
static size_t g_count;

static void test_visit(size_t v, void *ctx)
{
    (void)ctx;
    g_visited[g_count++] = v;
}

static void test_add_has_remove(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    scl_graph_t g;
    SCL_EXPECT_OK(tr, scl_graph_init(alloc, &g, 5));
    SCL_EXPECT_OK(tr, scl_graph_add_edge(alloc, &g, 0, 1, 1));
    SCL_EXPECT_OK(tr, scl_graph_add_edge(alloc, &g, 0, 2, 1));
    SCL_EXPECT_OK(tr, scl_graph_add_edge(alloc, &g, 1, 2, 1));
    SCL_EXPECT_TRUE(tr, scl_graph_has_edge(&g, 0, 1));
    SCL_EXPECT_FALSE(tr, scl_graph_has_edge(&g, 1, 0));
    SCL_EXPECT_EQ_SZ(tr, scl_graph_edge_count(&g), 3);
    SCL_EXPECT_OK(tr, scl_graph_remove_edge(alloc, &g, 0, 1));
    SCL_EXPECT_FALSE(tr, scl_graph_has_edge(&g, 0, 1));
    scl_graph_destroy(alloc, &g);
}

static void test_bounds(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    scl_graph_t g;
    SCL_EXPECT_OK(tr, scl_graph_init(alloc, &g, 3));
    SCL_EXPECT_EQ_I(tr, scl_graph_add_edge(alloc, &g, 5, 0, 1), SCL_ERR_INVALID_INDEX);
    SCL_EXPECT_FALSE(tr, scl_graph_has_edge(&g, 5, 0));
    scl_graph_destroy(alloc, &g);
}

static void test_dfs_bfs(scl_test_runner_t *tr)
{
    scl_allocator_t *alloc = scl_allocator_default();
    scl_graph_t g;
    SCL_EXPECT_OK(tr, scl_graph_init(alloc, &g, 4));
    SCL_EXPECT_OK(tr, scl_graph_add_edge(alloc, &g, 0, 1, 1));
    SCL_EXPECT_OK(tr, scl_graph_add_edge(alloc, &g, 0, 2, 1));
    SCL_EXPECT_OK(tr, scl_graph_add_edge(alloc, &g, 1, 3, 1));
    g_count = 0;
    SCL_EXPECT_OK(tr, scl_graph_dfs(alloc, &g, 0, test_visit, NULL));
    g_count = 0;
    SCL_EXPECT_OK(tr, scl_graph_bfs(alloc, &g, 0, test_visit, NULL));
    scl_graph_destroy(alloc, &g);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("scl_graph tests");
    test_add_has_remove(&tr);
    test_bounds(&tr);
    test_dfs_bfs(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
