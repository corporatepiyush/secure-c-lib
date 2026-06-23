#include "scl_test.h"
#include "scl_concurrent_lru.h"
#include "scl_pthread.h"
#include "scl_atomic.h"

#define NTHREADS 4
#define OPS_PER_THREAD 200

static void test_clru_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("CLRU: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_lru_t c;
    scl_error_t err = scl_clru_init(alloc, &c, sizeof(int), sizeof(int), 10);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_clru_count(&c), 0);
    scl_clru_destroy(alloc, &c);
}

static void test_clru_put_get(scl_test_runner_t *tr) {
    scl_test_group("CLRU: put and get");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_lru_t c;
    scl_clru_init(alloc, &c, sizeof(int), sizeof(int), 10);

    int k = 1, v = 100;
    SCL_EXPECT_OK(tr, scl_clru_put(alloc, &c, &k, &v));
    SCL_EXPECT_TRUE(tr, scl_clru_contains(&c, &k));

    int out = 0;
    SCL_EXPECT_OK(tr, scl_clru_get(&c, &k, &out));
    SCL_EXPECT_EQ_I(tr, out, 100);

    scl_clru_destroy(alloc, &c);
}

static void test_clru_remove(scl_test_runner_t *tr) {
    scl_test_group("CLRU: remove");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_lru_t c;
    scl_clru_init(alloc, &c, sizeof(int), sizeof(int), 10);

    int k = 5, v = 50;
    scl_clru_put(alloc, &c, &k, &v);
    SCL_EXPECT_TRUE(tr, scl_clru_contains(&c, &k));
    SCL_EXPECT_OK(tr, scl_clru_remove(alloc, &c, &k));
    SCL_EXPECT_FALSE(tr, scl_clru_contains(&c, &k));

    scl_clru_destroy(alloc, &c);
}

static void test_clru_missing(scl_test_runner_t *tr) {
    scl_test_group("CLRU: get missing returns error");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_lru_t c;
    scl_clru_init(alloc, &c, sizeof(int), sizeof(int), 10);
    int k = 999, out;
    SCL_EXPECT_TRUE(tr, scl_clru_get(&c, &k, &out) != SCL_OK);
    scl_clru_destroy(alloc, &c);
}

typedef struct { scl_concurrent_lru_t *c; int base; } clru_arg_t;

static void *clru_put_thread(void *arg) {
    clru_arg_t *a = arg;
    scl_allocator_t *alloc = scl_allocator_default();
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int k = a->base + i;
        int v = k * 2;
        scl_clru_put(alloc, a->c, &k, &v);
    }
    return NULL;
}

static void test_clru_concurrent_put(scl_test_runner_t *tr) {
    scl_test_group("CLRU: concurrent puts");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_lru_t c;
    scl_clru_init(alloc, &c, sizeof(int), sizeof(int), (size_t)(NTHREADS * OPS_PER_THREAD));

    pthread_t threads[NTHREADS];
    clru_arg_t args[NTHREADS];
    for (int i = 0; i < NTHREADS; i++) {
        args[i] = (clru_arg_t){.c = &c, .base = i * OPS_PER_THREAD};
        pthread_create(&threads[i], NULL, clru_put_thread, &args[i]);
    }
    for (int i = 0; i < NTHREADS; i++)
        pthread_join(threads[i], NULL);

    SCL_EXPECT_EQ_SZ(tr, scl_clru_count(&c), (size_t)(NTHREADS * OPS_PER_THREAD));
    scl_clru_destroy(alloc, &c);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_clru_init_destroy(&tr);
    test_clru_put_get(&tr);
    test_clru_remove(&tr);
    test_clru_missing(&tr);
    test_clru_concurrent_put(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
