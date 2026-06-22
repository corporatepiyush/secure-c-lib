#include "concurrent_bloom.h"
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
    scl_atomic_bloom_t bf;
    assert(scl_atomic_bloom_init(scl_allocator_default(), &bf, 100, 0.01, NULL) == SCL_OK);
    assert(scl_atomic_bloom_count(&bf) == 0);
    scl_atomic_bloom_destroy(scl_allocator_default(), &bf);
    PASS();
}

static void test_insert_contains(void)
{
    TEST("insert and maybe_contains");
    scl_atomic_bloom_t bf;
    scl_atomic_bloom_init(scl_allocator_default(), &bf, 100, 0.01, NULL);
    int keys[] = {10, 20, 30, 40, 50};
    for (int i = 0; i < 5; i++)
        assert(scl_atomic_bloom_insert(&bf, &keys[i], sizeof(int)) == SCL_OK);
    assert(scl_atomic_bloom_count(&bf) == 5);
    for (int i = 0; i < 5; i++)
        assert(scl_atomic_bloom_maybe_contains(&bf, &keys[i], sizeof(int)));
    scl_atomic_bloom_destroy(scl_allocator_default(), &bf);
    PASS();
}

static void test_clear(void)
{
    TEST("clear");
    scl_atomic_bloom_t bf;
    scl_atomic_bloom_init(scl_allocator_default(), &bf, 100, 0.01, NULL);
    int v = 42;
    scl_atomic_bloom_insert(&bf, &v, sizeof(int));
    assert(scl_atomic_bloom_maybe_contains(&bf, &v, sizeof(int)));
    scl_atomic_bloom_clear(&bf);
    assert(scl_atomic_bloom_count(&bf) == 0);
    scl_atomic_bloom_destroy(scl_allocator_default(), &bf);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_atomic_bloom_init(scl_allocator_default(), NULL, 100, 0.01, NULL) == SCL_ERR_NULL_PTR);
    scl_atomic_bloom_destroy(scl_allocator_default(), NULL);
    PASS();
}

static void *insert_thread(void *arg)
{
    scl_atomic_bloom_t *bf = (scl_atomic_bloom_t *)arg;
    for (int i = 0; i < 50; i++) scl_atomic_bloom_insert(bf, &i, sizeof(int));
    return NULL;
}

static void test_concurrent_insert(void)
{
    TEST("concurrent insert 2 threads");
    scl_atomic_bloom_t bf;
    scl_atomic_bloom_init(scl_allocator_default(), &bf, 200, 0.01, NULL);
    pthread_t t1, t2;
    pthread_create(&t1, NULL, insert_thread, &bf);
    pthread_create(&t2, NULL, insert_thread, &bf);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    assert(scl_atomic_bloom_count(&bf) == 100);
    for (int i = 0; i < 50; i++) assert(scl_atomic_bloom_maybe_contains(&bf, &i, sizeof(int)));
    scl_atomic_bloom_destroy(scl_allocator_default(), &bf);
    PASS();
}

int main(void)
{
    printf("=== scl_bloom tests ===\n");
    test_init_destroy();
    test_insert_contains();
    test_clear();
    test_null();
    test_concurrent_insert();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
