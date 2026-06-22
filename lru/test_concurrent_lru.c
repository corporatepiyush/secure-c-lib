#include "concurrent_lru.h"
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
    scl_concurrent_lru_t cache;
    assert(scl_concurrent_lru_init(&cache, sizeof(int), sizeof(int), 10) == SCL_OK);
    assert(scl_concurrent_lru_count(&cache) == 0);
    scl_concurrent_lru_destroy(&cache);
    PASS();
}

static void test_put_get_contains_remove(void)
{
    TEST("put, get, contains, remove");
    scl_concurrent_lru_t cache;
    scl_concurrent_lru_init(&cache, sizeof(int), sizeof(int), 10);
    int k, v;
    for (int i = 0; i < 10; i++) {
        k = i; v = i * 10;
        assert(scl_concurrent_lru_put(&cache, &k, &v) == SCL_OK);
    }
    assert(scl_concurrent_lru_count(&cache) == 10);
    for (int i = 0; i < 10; i++) {
        int out;
        k = i;
        assert(scl_concurrent_lru_get(&cache, &k, &out) == SCL_OK);
        assert(out == i * 10);
        assert(scl_concurrent_lru_contains(&cache, &k));
    }
    k = 11; assert(!scl_concurrent_lru_contains(&cache, &k));
    assert(scl_concurrent_lru_get(&cache, &k, &v) == SCL_ERR_NOT_FOUND);
    k = 0; assert(scl_concurrent_lru_remove(&cache, &k) == SCL_OK);
    assert(scl_concurrent_lru_count(&cache) == 9);
    assert(!scl_concurrent_lru_contains(&cache, &k));
    scl_concurrent_lru_destroy(&cache);
    PASS();
}

static void test_eviction(void)
{
    TEST("LRU eviction");
    scl_concurrent_lru_t cache;
    scl_concurrent_lru_init(&cache, sizeof(int), sizeof(int), 3);
    int k, v;
    for (int i = 0; i < 3; i++) { k = i; v = i; scl_concurrent_lru_put(&cache, &k, &v); }
    k = 0; scl_concurrent_lru_get(&cache, &k, &v);
    k = 3; v = 3; scl_concurrent_lru_put(&cache, &k, &v);
    assert(scl_concurrent_lru_contains(&cache, &k));
    assert(!scl_concurrent_lru_contains(&cache, &(int){1}));
    scl_concurrent_lru_destroy(&cache);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_concurrent_lru_init(NULL, sizeof(int), sizeof(int), 10) == SCL_ERR_NULL_PTR);
    scl_concurrent_lru_destroy(NULL);
    PASS();
}

static void *put_thread(void *arg)
{
    scl_concurrent_lru_t *cache = (scl_concurrent_lru_t *)arg;
    for (int i = 0; i < 30; i++) {
        int v = i;
        scl_concurrent_lru_put(cache, &v, &v);
    }
    return NULL;
}

static void test_concurrent_put(void)
{
    TEST("concurrent put 2 threads");
    scl_concurrent_lru_t cache;
    scl_concurrent_lru_init(&cache, sizeof(int), sizeof(int), 100);
    pthread_t t1, t2;
    pthread_create(&t1, NULL, put_thread, &cache);
    pthread_create(&t2, NULL, put_thread, &cache);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    assert(scl_concurrent_lru_count(&cache) <= 100);
    scl_concurrent_lru_destroy(&cache);
    PASS();
}

int main(void)
{
    printf("=== scl_concurrent_lru tests ===\n");
    test_init_destroy();
    test_put_get_contains_remove();
    test_eviction();
    test_null();
    test_concurrent_put();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
