#include "scl_test.h"
#include "scl_concurrent_graph.h"
#include "scl_pthread.h"
#include "scl_atomic.h"

#define NTHREADS 4
#define NVERTICES 64

static void test_cgraph_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("CGraph: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_graph_t g;
    scl_error_t err = scl_cgraph_init(alloc, &g, 5);
    SCL_EXPECT_OK(tr, err);
    scl_cgraph_destroy(alloc, &g);
}

static void test_cgraph_add_edge(scl_test_runner_t *tr) {
    scl_test_group("CGraph: add edge");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_graph_t g;
    scl_cgraph_init(alloc, &g, 5);

    SCL_EXPECT_OK(tr, scl_cgraph_add_edge(alloc, &g, 0, 1, 5));
    SCL_EXPECT_TRUE(tr, scl_cgraph_has_edge(&g, 0, 1));

    scl_cgraph_destroy(alloc, &g);
}

static void test_cgraph_remove_edge(scl_test_runner_t *tr) {
    scl_test_group("CGraph: remove edge");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_graph_t g;
    scl_cgraph_init(alloc, &g, 5);

    scl_cgraph_add_edge(alloc, &g, 0, 1, 5);
    SCL_EXPECT_TRUE(tr, scl_cgraph_has_edge(&g, 0, 1));
    SCL_EXPECT_OK(tr, scl_cgraph_remove_edge(alloc, &g, 0, 1));
    SCL_EXPECT_FALSE(tr, scl_cgraph_has_edge(&g, 0, 1));

    scl_cgraph_destroy(alloc, &g);
}

static void test_cgraph_no_edge(scl_test_runner_t *tr) {
    scl_test_group("CGraph: no edge returns false");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_graph_t g;
    scl_cgraph_init(alloc, &g, 5);
    SCL_EXPECT_FALSE(tr, scl_cgraph_has_edge(&g, 0, 1));
    scl_cgraph_destroy(alloc, &g);
}

typedef struct { scl_concurrent_graph_t *g; } cgraph_arg_t;

static void *cgraph_add_edge_thread(void *arg) {
    scl_concurrent_graph_t *g = arg;
    scl_allocator_t *alloc = scl_allocator_default();
    for (size_t from = 0; from < NVERTICES; from++) {
        size_t to = (from + 1) % NVERTICES;
        scl_cgraph_add_edge(alloc, g, from, to, 1);
    }
    return NULL;
}

static void test_cgraph_concurrent_add(scl_test_runner_t *tr) {
    scl_test_group("CGraph: concurrent add_edge");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_graph_t g;
    scl_cgraph_init(alloc, &g, NVERTICES);

    pthread_t threads[NTHREADS];
    for (int i = 0; i < NTHREADS; i++)
        pthread_create(&threads[i], NULL, cgraph_add_edge_thread, &g);
    for (int i = 0; i < NTHREADS; i++)
        pthread_join(threads[i], NULL);

    for (size_t from = 0; from < NVERTICES; from++) {
        size_t to = (from + 1) % NVERTICES;
        SCL_EXPECT_TRUE(tr, scl_cgraph_has_edge(&g, from, to));
    }
    scl_cgraph_destroy(alloc, &g);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_cgraph_init_destroy(&tr);
    test_cgraph_add_edge(&tr);
    test_cgraph_remove_edge(&tr);
    test_cgraph_no_edge(&tr);
    test_cgraph_concurrent_add(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
