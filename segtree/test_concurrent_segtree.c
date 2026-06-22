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

static void sum_op(void *out, const void *a, const void *b) { *(int64_t*)out = *(const int64_t*)a + *(const int64_t*)b; }

static void test_init_destroy(void)
{
    TEST("init and destroy");
    int64_t data[] = {1, 2, 3, 4, 5};
    scl_atomic_segtree_t st;
    assert(scl_atomic_segtree_init(scl_allocator_default(), &st, 5, sizeof(int64_t), data, sum_op) == SCL_OK);
    scl_atomic_segtree_destroy(scl_allocator_default(), &st);
    PASS();
}

static void test_query_update(void)
{
    TEST("query and update");
    int64_t data[] = {1, 2, 3, 4, 5};
    scl_atomic_segtree_t st;
    scl_atomic_segtree_init(scl_allocator_default(), &st, 5, sizeof(int64_t), data, sum_op);
    int64_t out;
    assert(scl_atomic_segtree_query(&st, 0, 4, &out) == SCL_OK && out == 15);
    assert(scl_atomic_segtree_query(&st, 1, 3, &out) == SCL_OK && out == 9);
    int64_t upd = 10;
    assert(scl_atomic_segtree_update(scl_allocator_default(), &st, 2, &upd) == SCL_OK);
    assert(scl_atomic_segtree_query(&st, 0, 4, &out) == SCL_OK && out == 22);
    scl_atomic_segtree_destroy(scl_allocator_default(), &st);
    PASS();
}

static void test_invalid(void)
{
    TEST("invalid args");
    scl_atomic_segtree_t st;
    scl_atomic_segtree_init(scl_allocator_default(), &st, 3, sizeof(int64_t), NULL, sum_op);
    int64_t out;
    assert(scl_atomic_segtree_query(&st, 5, 6, &out) == SCL_ERR_INVALID_INDEX);
    int64_t upd = 1;
    assert(scl_atomic_segtree_update(scl_allocator_default(), &st, 10, &upd) == SCL_ERR_INVALID_INDEX);
    scl_atomic_segtree_destroy(scl_allocator_default(), &st);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_atomic_segtree_init(scl_allocator_default(), NULL, 5, sizeof(int64_t), NULL, sum_op) == SCL_ERR_NULL_PTR);
    scl_atomic_segtree_destroy(scl_allocator_default(), NULL);
    PASS();
}

static void *update_thread(void *arg)
{
    scl_atomic_segtree_t *st = (scl_atomic_segtree_t *)arg;
    for (size_t i = 0; i < 5; i++) {
        int64_t delta = 1;
        scl_atomic_segtree_update(scl_allocator_default(), st, i, &delta);
    }
    return NULL;
}

static void test_concurrent_update(void)
{
    TEST("concurrent update 2 threads");
    int64_t data[] = {0, 0, 0, 0, 0};
    scl_atomic_segtree_t st;
    scl_atomic_segtree_init(scl_allocator_default(), &st, 5, sizeof(int64_t), data, sum_op);
    pthread_t t1, t2;
    pthread_create(&t1, NULL, update_thread, &st);
    pthread_create(&t2, NULL, update_thread, &st);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    int64_t out;
    scl_atomic_segtree_query(&st, 0, 4, &out);
    scl_atomic_segtree_destroy(scl_allocator_default(), &st);
    PASS();
}

int main(void)
{
    printf("=== scl_atomic_segtree tests ===\n");
    test_init_destroy();
    test_query_update();
    test_invalid();
    test_null();
    test_concurrent_update();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
