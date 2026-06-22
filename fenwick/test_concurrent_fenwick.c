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

static void add_int(void *out, const void *a, const void *b) { *(int64_t*)out = *(const int64_t*)a + *(const int64_t*)b; }
static void sub_int(void *out, const void *a, const void *b) { *(int64_t*)out = *(const int64_t*)a - *(const int64_t*)b; }

static void test_init_destroy(void)
{
    TEST("init and destroy");
    scl_atomic_fenwick_t ft;
    assert(scl_atomic_fenwick_init(scl_allocator_default(), &ft, 5, sizeof(int64_t), NULL, add_int, sub_int) == SCL_OK);
    scl_atomic_fenwick_destroy(scl_allocator_default(), &ft);
    PASS();
}

static void test_update_prefix_range(void)
{
    TEST("update, prefix, range");
    int64_t data[] = {1, 2, 3, 4, 5};
    scl_atomic_fenwick_t ft;
    scl_atomic_fenwick_init(scl_allocator_default(), &ft, 5, sizeof(int64_t), data, add_int, sub_int);
    int64_t out;
    assert(scl_atomic_fenwick_prefix(&ft, 4, &out) == SCL_OK && out == 15);
    assert(scl_atomic_fenwick_prefix(&ft, 2, &out) == SCL_OK && out == 6);
    assert(scl_atomic_fenwick_range_query(&ft, 1, 3, &out) == SCL_OK && out == 9);
    int64_t delta = 10;
    assert(scl_atomic_fenwick_update(scl_allocator_default(), &ft, 2, &delta) == SCL_OK);
    assert(scl_atomic_fenwick_prefix(&ft, 4, &out) == SCL_OK && out == 25);
    assert(scl_atomic_fenwick_range_query(&ft, 1, 3, &out) == SCL_OK && out == 19);
    scl_atomic_fenwick_destroy(scl_allocator_default(), &ft);
    PASS();
}

static void test_invalid(void)
{
    TEST("invalid args");
    scl_atomic_fenwick_t ft;
    scl_atomic_fenwick_init(scl_allocator_default(), &ft, 3, sizeof(int64_t), NULL, add_int, sub_int);
    int64_t out;
    assert(scl_atomic_fenwick_prefix(&ft, 10, &out) == SCL_ERR_INVALID_INDEX);
    assert(scl_atomic_fenwick_range_query(&ft, 1, 0, &out) == SCL_ERR_INVALID_INDEX);
    int64_t delta = 1;
    assert(scl_atomic_fenwick_update(scl_allocator_default(), &ft, 10, &delta) == SCL_ERR_INVALID_INDEX);
    scl_atomic_fenwick_destroy(scl_allocator_default(), &ft);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_atomic_fenwick_init(scl_allocator_default(), NULL, 5, sizeof(int64_t), NULL, add_int, sub_int) == SCL_ERR_NULL_PTR);
    scl_atomic_fenwick_destroy(scl_allocator_default(), NULL);
    PASS();
}

static void *update_thread(void *arg)
{
    scl_atomic_fenwick_t *ft = (scl_atomic_fenwick_t *)arg;
    int64_t delta = 1;
    for (size_t i = 0; i < 5; i++) scl_atomic_fenwick_update(scl_allocator_default(), ft, i, &delta);
    return NULL;
}

static void test_concurrent_update(void)
{
    TEST("concurrent update 2 threads");
    scl_atomic_fenwick_t ft;
    scl_atomic_fenwick_init(scl_allocator_default(), &ft, 5, sizeof(int64_t), NULL, add_int, sub_int);
    pthread_t t1, t2;
    pthread_create(&t1, NULL, update_thread, &ft);
    pthread_create(&t2, NULL, update_thread, &ft);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    int64_t out;
    scl_atomic_fenwick_prefix(&ft, 4, &out);
    scl_atomic_fenwick_destroy(scl_allocator_default(), &ft);
    PASS();
}

int main(void)
{
    printf("=== scl_atomic_fenwick tests ===\n");
    test_init_destroy();
    test_update_prefix_range();
    test_invalid();
    test_null();
    test_concurrent_update();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
