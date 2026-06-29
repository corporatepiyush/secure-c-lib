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

static void test_graph_sharded(scl_test_runner_t *tr) {
    scl_test_group("Graph: sharded node/edge stores + caller shard length");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_graph_t g;
    /* Caller-chosen shard length (cache tuning): 4 records per shard. 7 vertices
     * (0..6) so the slot-reuse edge below has a distinct destination, vertex 6. */
    SCL_EXPECT_OK(tr, scl_graph_init_ex(alloc, &g, 7, 4));

    /* All vertex records live in the nodes sharded array (1 per vertex). */
    SCL_EXPECT_EQ_SZ(tr, scl_sharded_array_count(&g.nodes), 7);
    SCL_EXPECT_EQ_SZ(tr, g.edges.shard_len, 4);   /* edges array honours the choice */

    /* Several out-edges from vertex 0 thread through the edge array. */
    for (int t = 1; t <= 5; t++) SCL_EXPECT_OK(tr, scl_graph_add_edge(alloc, &g, 0, (size_t)t, t));
    SCL_EXPECT_EQ_SZ(tr, scl_graph_edge_count(&g), 5);
    SCL_EXPECT_EQ_SZ(tr, scl_sharded_array_count(&g.edges), 5);

    /* Walk vertex 0's adjacency chain and collect destinations. */
    int seen[7] = {0};
    size_t n = 0;
    for (size_t e = scl_graph_adj_head(&g, 0); e != SCL_GRAPH_NIL; ) {
        const scl_graph_edge_t *ed = scl_graph_edge(&g, e);
        SCL_EXPECT_TRUE(tr, ed->to >= 1 && ed->to <= 5);
        seen[ed->to]++;
        n++;
        e = ed->next;
    }
    SCL_EXPECT_EQ_SZ(tr, n, 5);
    for (int t = 1; t <= 5; t++) SCL_EXPECT_EQ_I(tr, seen[t], 1);

    /* Remove one edge -> slot goes on the free list and is reused by the next
     * add (no growth of the edge array's element count). */
    SCL_EXPECT_OK(tr, scl_graph_remove_edge(alloc, &g, 0, 3));
    SCL_EXPECT_FALSE(tr, scl_graph_has_edge(&g, 0, 3));
    SCL_EXPECT_EQ_SZ(tr, scl_graph_edge_count(&g), 4);
    SCL_EXPECT_OK(tr, scl_graph_add_edge(alloc, &g, 0, 6, 60));
    SCL_EXPECT_EQ_SZ(tr, scl_graph_edge_count(&g), 5);
    SCL_EXPECT_EQ_SZ(tr, scl_sharded_array_count(&g.edges), 5);   /* slot reused, no append */
    SCL_EXPECT_TRUE(tr, scl_graph_has_edge(&g, 0, 6));
    scl_graph_destroy(alloc, &g);

    /* shard_len == 0 falls back to the default. */
    scl_graph_t d;
    SCL_EXPECT_OK(tr, scl_graph_init_ex(alloc, &d, 2, 0));
    SCL_EXPECT_EQ_SZ(tr, d.edges.shard_len, SCL_GRAPH_DEFAULT_SHARD_LEN);
    scl_graph_destroy(alloc, &d);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);
    test_graph_init_destroy(&tr);
    test_graph_add_edge(&tr);
    test_graph_remove_edge(&tr);
    test_graph_sharded(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
