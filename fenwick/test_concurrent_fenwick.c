#include "concurrent_fenwick.h"
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
    scl_concurrent_fenwick_t ft;
    assert(scl_concurrent_fenwick_init(&ft, NULL, 10) == SCL_OK);
    scl_concurrent_fenwick_destroy(&ft);
    PASS();
}

static void test_update_prefix_range(void)
{
    TEST("update, prefix, range");
    int64_t data[] = {1, 2, 3, 4, 5};
    scl_concurrent_fenwick_t ft;
    scl_concurrent_fenwick_init(&ft, data, 5);
    int64_t out;
    assert(scl_concurrent_fenwick_prefix(&ft, 4, &out) == SCL_OK && out == 15);
    assert(scl_concurrent_fenwick_prefix(&ft, 2, &out) == SCL_OK && out == 6);
    assert(scl_concurrent_fenwick_range(&ft, 1, 3, &out) == SCL_OK && out == 9);
    assert(scl_concurrent_fenwick_update(&ft, 2, 10) == SCL_OK);
    assert(scl_concurrent_fenwick_prefix(&ft, 4, &out) == SCL_OK && out == 25);
    assert(scl_concurrent_fenwick_range(&ft, 1, 3, &out) == SCL_OK && out == 19);
    scl_concurrent_fenwick_destroy(&ft);
    PASS();
}

static void test_invalid(void)
{
    TEST("invalid args");
    int64_t data[] = {1, 2, 3};
    scl_concurrent_fenwick_t ft;
    scl_concurrent_fenwick_init(&ft, data, 3);
    int64_t out;
    assert(scl_concurrent_fenwick_prefix(&ft, 10, &out) == SCL_ERR_INVALID_INDEX);
    assert(scl_concurrent_fenwick_range(&ft, 1, 0, &out) == SCL_ERR_INVALID_INDEX);
    assert(scl_concurrent_fenwick_update(&ft, 10, 1) == SCL_ERR_INVALID_INDEX);
    scl_concurrent_fenwick_destroy(&ft);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_concurrent_fenwick_init(NULL, NULL, 5) == SCL_ERR_NULL_PTR);
    scl_concurrent_fenwick_destroy(NULL);
    PASS();
}

static void *update_thread(void *arg)
{
    scl_concurrent_fenwick_t *ft = (scl_concurrent_fenwick_t *)arg;
    for (size_t i = 0; i < 5; i++) scl_concurrent_fenwick_update(ft, i, 1);
    return NULL;
}

static void test_concurrent_update(void)
{
    TEST("concurrent update 2 threads");
    scl_concurrent_fenwick_t ft;
    scl_concurrent_fenwick_init(&ft, NULL, 5);
    pthread_t t1, t2;
    pthread_create(&t1, NULL, update_thread, &ft);
    pthread_create(&t2, NULL, update_thread, &ft);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    int64_t out;
    scl_concurrent_fenwick_prefix(&ft, 4, &out);
    scl_concurrent_fenwick_destroy(&ft);
    PASS();
}

int main(void)
{
    printf("=== scl_concurrent_fenwick tests ===\n");
    test_init_destroy();
    test_update_prefix_range();
    test_invalid();
    test_null();
    test_concurrent_update();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
