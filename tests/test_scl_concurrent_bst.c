#include "scl_test.h"
#include "scl_concurrent_bst.h"
#include "scl_pthread.h"
#include "scl_atomic.h"

#define NTHREADS 4
#define OPS_PER_THREAD 500

static int int_cmp(const void *a, const void *b) {
    int va = *(int *)a, vb = *(int *)b;
    return (va > vb) - (va < vb);
}

static void test_cbst_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("CBST: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_bst_t t;
    scl_error_t err = scl_cbst_init(alloc, &t, sizeof(int), int_cmp);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_cbst_count(&t), 0);
    SCL_EXPECT_TRUE(tr, scl_cbst_empty(&t));
    scl_cbst_destroy(alloc, &t);
}

static void test_cbst_insert_find(scl_test_runner_t *tr) {
    scl_test_group("CBST: insert and find");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_bst_t t;
    scl_cbst_init(alloc, &t, sizeof(int), int_cmp);

    int v = 42;
    SCL_EXPECT_OK(tr, scl_cbst_insert(alloc, &t, &v));
    SCL_EXPECT_EQ_SZ(tr, scl_cbst_count(&t), 1);
    SCL_EXPECT_TRUE(tr, scl_cbst_contains(&t, &v));

    int out = 0;
    SCL_EXPECT_OK(tr, scl_cbst_find(&t, &v, &out));
    SCL_EXPECT_EQ_I(tr, out, 42);

    scl_cbst_destroy(alloc, &t);
}

static void test_cbst_remove(scl_test_runner_t *tr) {
    scl_test_group("CBST: remove");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_bst_t t;
    scl_cbst_init(alloc, &t, sizeof(int), int_cmp);

    int v = 7;
    scl_cbst_insert(alloc, &t, &v);
    SCL_EXPECT_TRUE(tr, scl_cbst_contains(&t, &v));
    SCL_EXPECT_OK(tr, scl_cbst_remove(alloc, &t, &v));
    SCL_EXPECT_FALSE(tr, scl_cbst_contains(&t, &v));
    SCL_EXPECT_EQ_SZ(tr, scl_cbst_count(&t), 0);

    scl_cbst_destroy(alloc, &t);
}

static void test_cbst_missing(scl_test_runner_t *tr) {
    scl_test_group("CBST: find missing returns error");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_bst_t t;
    scl_cbst_init(alloc, &t, sizeof(int), int_cmp);
    int key = 999, out;
    SCL_EXPECT_TRUE(tr, scl_cbst_find(&t, &key, &out) != SCL_OK);
    scl_cbst_destroy(alloc, &t);
}

static void test_cbst_multiple(scl_test_runner_t *tr) {
    scl_test_group("CBST: multiple entries");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_bst_t t;
    scl_cbst_init(alloc, &t, sizeof(int), int_cmp);

    for (int i = 0; i < 50; i++)
        scl_cbst_insert(alloc, &t, &i);
    SCL_EXPECT_EQ_SZ(tr, scl_cbst_count(&t), 50);

    for (int i = 0; i < 50; i++) {
        int out = -1;
        SCL_EXPECT_OK(tr, scl_cbst_find(&t, &i, &out));
        SCL_EXPECT_EQ_I(tr, out, i);
    }
    scl_cbst_destroy(alloc, &t);
}

typedef struct { scl_concurrent_bst_t *t; int base; } cbst_arg_t;

static void *cbst_insert_thread(void *arg) {
    cbst_arg_t *a = arg;
    scl_allocator_t *alloc = scl_allocator_default();
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int k = a->base + i;
        scl_cbst_insert(alloc, a->t, &k);
    }
    return NULL;
}

static void test_cbst_concurrent_insert(scl_test_runner_t *tr) {
    scl_test_group("CBST: concurrent inserts");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_bst_t t;
    scl_cbst_init(alloc, &t, sizeof(int), int_cmp);

    pthread_t threads[NTHREADS];
    cbst_arg_t args[NTHREADS];
    for (int i = 0; i < NTHREADS; i++) {
        args[i] = (cbst_arg_t){.t = &t, .base = i * OPS_PER_THREAD};
        pthread_create(&threads[i], NULL, cbst_insert_thread, &args[i]);
    }
    for (int i = 0; i < NTHREADS; i++)
        pthread_join(threads[i], NULL);

    SCL_EXPECT_EQ_SZ(tr, scl_cbst_count(&t), (size_t)(NTHREADS * OPS_PER_THREAD));
    scl_cbst_destroy(alloc, &t);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_cbst_init_destroy(&tr);
    test_cbst_insert_find(&tr);
    test_cbst_remove(&tr);
    test_cbst_missing(&tr);
    test_cbst_multiple(&tr);
    test_cbst_concurrent_insert(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
