#include "scl_test.h"
#include "scl_graph.h"

static void test_graph_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("Graph: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_graph_t g;
    scl_error_t err = scl_graph_init(alloc, &g, 5);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_graph_vertex_count(&g), 5);
    scl_graph_destroy(alloc, &g);
}

static void test_graph_add_edge(scl_test_runner_t *tr) {
    scl_test_group("Graph: add edge");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_graph_t g;
    scl_graph_init(alloc, &g, 5);
    scl_error_t err = scl_graph_add_edge(alloc, &g, 0, 1, 5);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_TRUE(tr, scl_graph_has_edge(&g, 0, 1));
    scl_graph_destroy(alloc, &g);
}

static void test_graph_remove_edge(scl_test_runner_t *tr) {
    scl_test_group("Graph: remove edge");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_graph_t g;
    scl_graph_init(alloc, &g, 5);
    scl_graph_add_edge(alloc, &g, 0, 1, 5);
    scl_error_t err = scl_graph_remove_edge(alloc, &g, 0, 1);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_FALSE(tr, scl_graph_has_edge(&g, 0, 1));
    scl_graph_destroy(alloc, &g);
}

static void test_graph_shard_cap(scl_test_runner_t *tr) {
    scl_test_group("Graph: caller-chosen shard capacity");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_graph_t g;
    /* shard_cap = 8: a vertex's first edge allocates 8 slots, then doubles. */
    SCL_EXPECT_OK(tr, scl_graph_init_ex(alloc, &g, 3, 8));
    SCL_EXPECT_EQ_SZ(tr, g.shard_cap, 8);

    /* No edges yet -> shard allocated lazily (no memory reserved). */
    SCL_EXPECT_NULL(tr, g.adj[0].edges);
    SCL_EXPECT_EQ_SZ(tr, g.adj[0].cap, 0);

    for (int i = 0; i < 8; i++) SCL_EXPECT_OK(tr, scl_graph_add_edge(alloc, &g, 0, 1, i));
    SCL_EXPECT_EQ_SZ(tr, g.adj[0].cap, 8);      /* exactly the chosen size, no realloc yet */
    SCL_EXPECT_EQ_SZ(tr, g.adj[0].count, 8);

    SCL_EXPECT_OK(tr, scl_graph_add_edge(alloc, &g, 0, 2, 99));
    SCL_EXPECT_EQ_SZ(tr, g.adj[0].cap, 16);     /* grows by doubling */
    SCL_EXPECT_EQ_SZ(tr, g.adj[0].count, 9);
    scl_graph_destroy(alloc, &g);

    /* shard_cap == 0 falls back to the default. */
    scl_graph_t d;
    SCL_EXPECT_OK(tr, scl_graph_init_ex(alloc, &d, 2, 0));
    SCL_EXPECT_EQ_SZ(tr, d.shard_cap, SCL_GRAPH_DEFAULT_SHARD_CAP);
    scl_graph_destroy(alloc, &d);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);
    test_graph_init_destroy(&tr);
    test_graph_add_edge(&tr);
    test_graph_remove_edge(&tr);
    test_graph_shard_cap(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
