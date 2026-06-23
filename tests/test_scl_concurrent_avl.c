#include "scl_test.h"
#include "scl_concurrent_avl.h"
#include "scl_pthread.h"
#include "scl_atomic.h"

#define NTHREADS 4
#define OPS_PER_THREAD 500

static int int_cmp(const void *a, const void *b) {
    int va = *(int *)a, vb = *(int *)b;
    return (va > vb) - (va < vb);
}

static void test_cavl_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("CAVL: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_avl_t t;
    scl_error_t err = scl_cavl_init(alloc, &t, sizeof(int), int_cmp);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_cavl_count(&t), 0);
    SCL_EXPECT_TRUE(tr, scl_cavl_empty(&t));
    scl_cavl_destroy(alloc, &t);
}

static void test_cavl_insert_find(scl_test_runner_t *tr) {
    scl_test_group("CAVL: insert and find");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_avl_t t;
    scl_cavl_init(alloc, &t, sizeof(int), int_cmp);

    int v = 42;
    SCL_EXPECT_OK(tr, scl_cavl_insert(alloc, &t, &v));
    SCL_EXPECT_EQ_SZ(tr, scl_cavl_count(&t), 1);
    SCL_EXPECT_TRUE(tr, scl_cavl_contains(&t, &v));

    int out = 0;
    SCL_EXPECT_OK(tr, scl_cavl_find(&t, &v, &out));
    SCL_EXPECT_EQ_I(tr, out, 42);

    scl_cavl_destroy(alloc, &t);
}

static void test_cavl_remove(scl_test_runner_t *tr) {
    scl_test_group("CAVL: remove");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_avl_t t;
    scl_cavl_init(alloc, &t, sizeof(int), int_cmp);

    int v = 7;
    scl_cavl_insert(alloc, &t, &v);
    SCL_EXPECT_TRUE(tr, scl_cavl_contains(&t, &v));
    SCL_EXPECT_OK(tr, scl_cavl_remove(alloc, &t, &v));
    SCL_EXPECT_FALSE(tr, scl_cavl_contains(&t, &v));
    SCL_EXPECT_EQ_SZ(tr, scl_cavl_count(&t), 0);

    scl_cavl_destroy(alloc, &t);
}

static void test_cavl_missing(scl_test_runner_t *tr) {
    scl_test_group("CAVL: find missing returns error");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_avl_t t;
    scl_cavl_init(alloc, &t, sizeof(int), int_cmp);
    int key = 999, out;
    SCL_EXPECT_TRUE(tr, scl_cavl_find(&t, &key, &out) != SCL_OK);
    scl_cavl_destroy(alloc, &t);
}

static void test_cavl_multiple(scl_test_runner_t *tr) {
    scl_test_group("CAVL: multiple entries");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_avl_t t;
    scl_cavl_init(alloc, &t, sizeof(int), int_cmp);

    for (int i = 0; i < 50; i++)
        scl_cavl_insert(alloc, &t, &i);
    SCL_EXPECT_EQ_SZ(tr, scl_cavl_count(&t), 50);

    for (int i = 0; i < 50; i++) {
        int out = -1;
        SCL_EXPECT_OK(tr, scl_cavl_find(&t, &i, &out));
        SCL_EXPECT_EQ_I(tr, out, i);
    }
    scl_cavl_destroy(alloc, &t);
}

typedef struct { scl_concurrent_avl_t *t; int base; } cavl_arg_t;

static void *cavl_insert_thread(void *arg) {
    cavl_arg_t *a = arg;
    scl_allocator_t *alloc = scl_allocator_default();
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int k = a->base + i;
        scl_cavl_insert(alloc, a->t, &k);
    }
    return NULL;
}

static void test_cavl_concurrent_insert(scl_test_runner_t *tr) {
    scl_test_group("CAVL: concurrent inserts");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_avl_t t;
    scl_cavl_init(alloc, &t, sizeof(int), int_cmp);

    pthread_t threads[NTHREADS];
    cavl_arg_t args[NTHREADS];
    for (int i = 0; i < NTHREADS; i++) {
        args[i] = (cavl_arg_t){.t = &t, .base = i * OPS_PER_THREAD};
        pthread_create(&threads[i], NULL, cavl_insert_thread, &args[i]);
    }
    for (int i = 0; i < NTHREADS; i++)
        pthread_join(threads[i], NULL);

    SCL_EXPECT_EQ_SZ(tr, scl_cavl_count(&t), (size_t)(NTHREADS * OPS_PER_THREAD));
    scl_cavl_destroy(alloc, &t);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_cavl_init_destroy(&tr);
    test_cavl_insert_find(&tr);
    test_cavl_remove(&tr);
    test_cavl_missing(&tr);
    test_cavl_multiple(&tr);
    test_cavl_concurrent_insert(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
