#include "concurrent_graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static void test_init_destroy(void)
{
    TEST("init and destroy");
    scl_concurrent_graph_t g;
    assert(scl_concurrent_graph_init(&g, 5) == SCL_OK);
    scl_concurrent_graph_destroy(&g);
    PASS();
}

static void test_add_remove_has_edge(void)
{
    TEST("add/has/remove edge");
    scl_concurrent_graph_t g;
    scl_concurrent_graph_init(&g, 5);
    assert(scl_concurrent_graph_add_edge(&g, 0, 1, 10) == SCL_OK);
    assert(scl_concurrent_graph_add_edge(&g, 1, 2, 20) == SCL_OK);
    assert(scl_concurrent_graph_has_edge(&g, 0, 1));
    assert(scl_concurrent_graph_has_edge(&g, 1, 2));
    assert(!scl_concurrent_graph_has_edge(&g, 0, 2));
    assert(scl_concurrent_graph_remove_edge(&g, 0, 1) == SCL_OK);
    assert(!scl_concurrent_graph_has_edge(&g, 0, 1));
    assert(scl_concurrent_graph_remove_edge(&g, 0, 1) == SCL_ERR_NOT_FOUND);
    scl_concurrent_graph_destroy(&g);
    PASS();
}

static void test_invalid(void)
{
    TEST("invalid args");
    scl_concurrent_graph_t g;
    scl_concurrent_graph_init(&g, 3);
    assert(scl_concurrent_graph_add_edge(&g, 5, 0, 1) == SCL_ERR_INVALID_INDEX);
    assert(!scl_concurrent_graph_has_edge(&g, 5, 0));
    scl_concurrent_graph_destroy(&g);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_concurrent_graph_init(NULL, 5) == SCL_ERR_NULL_PTR);
    scl_concurrent_graph_destroy(NULL);
    PASS();
}

static void *add_edge_thread(void *arg)
{
    scl_concurrent_graph_t *g = (scl_concurrent_graph_t *)arg;
    for (size_t i = 0; i < 50; i++) scl_concurrent_graph_add_edge(g, 0, 1, 1);
    return NULL;
}

static void test_concurrent_add(void)
{
    TEST("concurrent add edge 2 threads");
    scl_concurrent_graph_t g;
    scl_concurrent_graph_init(&g, 5);
    pthread_t t1, t2;
    pthread_create(&t1, NULL, add_edge_thread, &g);
    pthread_create(&t2, NULL, add_edge_thread, &g);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    assert(scl_concurrent_graph_has_edge(&g, 0, 1));
    scl_concurrent_graph_destroy(&g);
    PASS();
}

int main(void)
{
    printf("=== scl_concurrent_graph tests ===\n");
    test_init_destroy();
    test_add_remove_has_edge();
    test_invalid();
    test_null();
    test_concurrent_add();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
