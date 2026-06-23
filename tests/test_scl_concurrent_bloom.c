#include "scl_test.h"
#include "scl_concurrent_bloom.h"
#include "scl_bloom.h"
#include <pthread.h>
#include <stdatomic.h>

#define NTHREADS 4
#define OPS_PER_THREAD 500

static void test_cbloom_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("CBloom: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_bloom_t bf;
    scl_error_t err = scl_cbloom_init(alloc, &bf, 100, 0.01, scl_bloom_hash_murmur);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_cbloom_count(&bf), 0);
    scl_cbloom_destroy(alloc, &bf);
}

static void test_cbloom_insert_contains(scl_test_runner_t *tr) {
    scl_test_group("CBloom: insert and contains");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_bloom_t bf;
    scl_cbloom_init(alloc, &bf, 100, 0.01, scl_bloom_hash_murmur);

    const char *hello = "hello";
    const char *world = "world";
    SCL_EXPECT_OK(tr, scl_cbloom_insert(&bf, hello, 5));
    SCL_EXPECT_OK(tr, scl_cbloom_insert(&bf, world, 5));
    SCL_EXPECT_TRUE(tr, scl_cbloom_maybe_contains(&bf, hello, 5));
    SCL_EXPECT_TRUE(tr, scl_cbloom_maybe_contains(&bf, world, 5));
    SCL_EXPECT_FALSE(tr, scl_cbloom_maybe_contains(&bf, "missing", 7));

    scl_cbloom_destroy(alloc, &bf);
}

static void test_cbloom_clear(scl_test_runner_t *tr) {
    scl_test_group("CBloom: clear");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_bloom_t bf;
    scl_cbloom_init(alloc, &bf, 100, 0.01, scl_bloom_hash_murmur);

    const char *data = "test";
    scl_cbloom_insert(&bf, data, 4);
    SCL_EXPECT_TRUE(tr, scl_cbloom_maybe_contains(&bf, data, 4));
    scl_cbloom_clear(&bf);
    SCL_EXPECT_EQ_SZ(tr, scl_cbloom_count(&bf), 0);

    scl_cbloom_destroy(alloc, &bf);
}

static void test_cbloom_multiple(scl_test_runner_t *tr) {
    scl_test_group("CBloom: multiple inserts");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_bloom_t bf;
    scl_cbloom_init(alloc, &bf, 1000, 0.01, scl_bloom_hash_murmur);

    for (int i = 0; i < 50; i++) {
        int v = i;
        scl_cbloom_insert(&bf, &v, sizeof(v));
    }
    for (int i = 0; i < 50; i++) {
        int v = i;
        SCL_EXPECT_TRUE(tr, scl_cbloom_maybe_contains(&bf, &v, sizeof(v)));
    }
    scl_cbloom_destroy(alloc, &bf);
}

typedef struct { scl_concurrent_bloom_t *bf; int base; } cbloom_arg_t;

static void *cbloom_insert_thread(void *arg) {
    cbloom_arg_t *a = arg;
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int k = a->base + i;
        scl_cbloom_insert(a->bf, &k, sizeof(k));
    }
    return NULL;
}

static void test_cbloom_concurrent_insert(scl_test_runner_t *tr) {
    scl_test_group("CBloom: concurrent inserts");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_bloom_t bf;
    scl_cbloom_init(alloc, &bf, (size_t)(NTHREADS * OPS_PER_THREAD * 2), 0.001, scl_bloom_hash_murmur);

    pthread_t threads[NTHREADS];
    cbloom_arg_t args[NTHREADS];
    for (int i = 0; i < NTHREADS; i++) {
        args[i] = (cbloom_arg_t){.bf = &bf, .base = i * OPS_PER_THREAD};
        pthread_create(&threads[i], NULL, cbloom_insert_thread, &args[i]);
    }
    for (int i = 0; i < NTHREADS; i++)
        pthread_join(threads[i], NULL);

    for (int i = 0; i < NTHREADS * OPS_PER_THREAD; i++) {
        int k = i;
        SCL_EXPECT_TRUE(tr, scl_cbloom_maybe_contains(&bf, &k, sizeof(k)));
    }
    scl_cbloom_destroy(alloc, &bf);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_cbloom_init_destroy(&tr);
    test_cbloom_insert_contains(&tr);
    test_cbloom_clear(&tr);
    test_cbloom_multiple(&tr);
    test_cbloom_concurrent_insert(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
