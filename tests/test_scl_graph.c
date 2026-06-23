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

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);
    test_graph_init_destroy(&tr);
    test_graph_add_edge(&tr);
    test_graph_remove_edge(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
