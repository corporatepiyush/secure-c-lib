#include "concurrent_lru.h"
#include "../../testlib/scl_test.h"
#include <pthread.h>

static void test_init_destroy(scl_test_runner_t *tr)
{
    scl_test_group("init/destroy");
    scl_atomic_lru_t cache;
    SCL_EXPECT_OK(tr, scl_atomic_lru_init(scl_allocator_default(), &cache, sizeof(int), sizeof(int), 10));
    SCL_EXPECT_EQ_SZ(tr, scl_atomic_lru_count(&cache), 0);
    scl_atomic_lru_destroy(scl_allocator_default(), &cache);
}

static void test_put_get_contains_remove(scl_test_runner_t *tr)
{
    scl_test_group("put/get/contains/remove");
    scl_atomic_lru_t cache;
    scl_atomic_lru_init(scl_allocator_default(), &cache, sizeof(int), sizeof(int), 10);
    for (int i = 0; i < 10; i++) {
        SCL_EXPECT_OK(tr, scl_atomic_lru_put(scl_allocator_default(), &cache, &i, &i));
    }
    SCL_EXPECT_EQ_SZ(tr, scl_atomic_lru_count(&cache), 10);
    for (int i = 0; i < 10; i++) {
        int out;
        SCL_EXPECT_OK(tr, scl_atomic_lru_get(&cache, &i, &out));
        SCL_EXPECT_TRUE(tr, scl_atomic_lru_contains(&cache, &i));
    }
    int k = 11;
    SCL_EXPECT_FALSE(tr, scl_atomic_lru_contains(&cache, &k));
    SCL_EXPECT_ERROR(tr, scl_atomic_lru_get(&cache, &k, &(int){0}), SCL_ERR_NOT_FOUND);
    k = 0;
    SCL_EXPECT_OK(tr, scl_atomic_lru_remove(scl_allocator_default(), &cache, &k));
    SCL_EXPECT_EQ_SZ(tr, scl_atomic_lru_count(&cache), 9);
    SCL_EXPECT_FALSE(tr, scl_atomic_lru_contains(&cache, &k));
    scl_atomic_lru_destroy(scl_allocator_default(), &cache);
}

static void test_eviction(scl_test_runner_t *tr)
{
    scl_test_group("eviction");
    scl_atomic_lru_t cache;
    scl_atomic_lru_init(scl_allocator_default(), &cache, sizeof(int), sizeof(int), 3);
    for (int i = 0; i < 3; i++) scl_atomic_lru_put(scl_allocator_default(), &cache, &i, &i);
    int k = 0, v;
    scl_atomic_lru_get(&cache, &k, &v);
    k = 3; v = 3;
    scl_atomic_lru_put(scl_allocator_default(), &cache, &k, &v);
    SCL_EXPECT_TRUE(tr, scl_atomic_lru_contains(&cache, &k));
    k = 1;
    SCL_EXPECT_FALSE(tr, scl_atomic_lru_contains(&cache, &k));
    scl_atomic_lru_destroy(scl_allocator_default(), &cache);
}

static void test_null(scl_test_runner_t *tr)
{
    scl_test_group("null checks");
    SCL_EXPECT_ERROR(tr, scl_atomic_lru_init(scl_allocator_default(), NULL, sizeof(int), sizeof(int), 10), SCL_ERR_NULL_PTR);
    scl_atomic_lru_destroy(scl_allocator_default(), NULL);
}

static void *put_thread(void *arg)
{
    scl_atomic_lru_t *cache = (scl_atomic_lru_t *)arg;
    for (int i = 0; i < 30; i++)
        scl_atomic_lru_put(scl_allocator_default(), cache, &i, &i);
    return NULL;
}

static void test_concurrent_put(scl_test_runner_t *tr)
{
    scl_test_group("concurrent put");
    scl_atomic_lru_t cache;
    scl_atomic_lru_init(scl_allocator_default(), &cache, sizeof(int), sizeof(int), 100);
    pthread_t t1, t2;
    pthread_create(&t1, NULL, put_thread, &cache);
    pthread_create(&t2, NULL, put_thread, &cache);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    SCL_EXPECT_TRUE(tr, scl_atomic_lru_count(&cache) <= 100);
    scl_atomic_lru_destroy(scl_allocator_default(), &cache);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    test_init_destroy(&tr);
    test_put_get_contains_remove(&tr);
    test_eviction(&tr);
    test_null(&tr);
    test_concurrent_put(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
