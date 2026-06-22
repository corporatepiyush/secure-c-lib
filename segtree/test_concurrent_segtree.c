#include "concurrent_segtree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <stdint.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static int64_t sum_op(int64_t a, int64_t b) { return a + b; }

static void test_init_destroy(void)
{
    TEST("init and destroy");
    int64_t data[] = {1, 2, 3, 4, 5};
    scl_concurrent_segtree_t st;
    assert(scl_concurrent_segtree_init(&st, data, 5, sum_op, 0) == SCL_OK);
    scl_concurrent_segtree_destroy(&st);
    PASS();
}

static void test_query_update(void)
{
    TEST("query and update");
    int64_t data[] = {1, 2, 3, 4, 5};
    scl_concurrent_segtree_t st;
    scl_concurrent_segtree_init(&st, data, 5, sum_op, 0);
    int64_t out;
    assert(scl_concurrent_segtree_query(&st, 0, 4, &out) == SCL_OK && out == 15);
    assert(scl_concurrent_segtree_query(&st, 1, 3, &out) == SCL_OK && out == 9);
    assert(scl_concurrent_segtree_update(&st, 2, 10) == SCL_OK);
    assert(scl_concurrent_segtree_query(&st, 0, 4, &out) == SCL_OK && out == 22);
    scl_concurrent_segtree_destroy(&st);
    PASS();
}

static void test_invalid(void)
{
    TEST("invalid args");
    int64_t data[] = {1, 2, 3};
    scl_concurrent_segtree_t st;
    scl_concurrent_segtree_init(&st, data, 3, sum_op, 0);
    int64_t out;
    assert(scl_concurrent_segtree_query(&st, 5, 6, &out) == SCL_ERR_INVALID_INDEX);
    assert(scl_concurrent_segtree_update(&st, 10, 1) == SCL_ERR_INVALID_INDEX);
    scl_concurrent_segtree_destroy(&st);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_concurrent_segtree_init(NULL, NULL, 5, sum_op, 0) == SCL_ERR_NULL_PTR);
    scl_concurrent_segtree_destroy(NULL);
    PASS();
}

static void *update_thread(void *arg)
{
    scl_concurrent_segtree_t *st = (scl_concurrent_segtree_t *)arg;
    for (size_t i = 0; i < 5; i++) scl_concurrent_segtree_update(st, i, 1);
    return NULL;
}

static void test_concurrent_update(void)
{
    TEST("concurrent update 2 threads");
    int64_t data[] = {0, 0, 0, 0, 0};
    scl_concurrent_segtree_t st;
    scl_concurrent_segtree_init(&st, data, 5, sum_op, 0);
    pthread_t t1, t2;
    pthread_create(&t1, NULL, update_thread, &st);
    pthread_create(&t2, NULL, update_thread, &st);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    int64_t out;
    scl_concurrent_segtree_query(&st, 0, 4, &out);
    scl_concurrent_segtree_destroy(&st);
    PASS();
}

int main(void)
{
    printf("=== scl_concurrent_segtree tests ===\n");
    test_init_destroy();
    test_query_update();
    test_invalid();
    test_null();
    test_concurrent_update();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
