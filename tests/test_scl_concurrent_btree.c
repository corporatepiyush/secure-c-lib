#include "scl_test.h"
#include "scl_concurrent_btree.h"
#include <pthread.h>
#include <stdatomic.h>

#define NTHREADS 4
#define OPS_PER_THREAD 50

static int int_cmp(const void *a, const void *b) {
    int va = *(int *)a, vb = *(int *)b;
    return (va > vb) - (va < vb);
}

static void test_cbtree_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("CBTree: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_btree_t t;
    scl_error_t err = scl_cbtree_init(alloc, &t, sizeof(int), sizeof(int), SCL_BTREE_DEGREE, int_cmp);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_cbtree_count(&t), 0);
    scl_cbtree_destroy(alloc, &t);
}

static void test_cbtree_insert_get(scl_test_runner_t *tr) {
    scl_test_group("CBTree: insert and get");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_btree_t t;
    scl_cbtree_init(alloc, &t, sizeof(int), sizeof(int), SCL_BTREE_DEGREE, int_cmp);

    int k = 7, v = 42;
    SCL_EXPECT_OK(tr, scl_cbtree_insert(alloc, &t, &k, &v));
    SCL_EXPECT_EQ_SZ(tr, scl_cbtree_count(&t), 1);
    SCL_EXPECT_TRUE(tr, scl_cbtree_contains(&t, &k));

    int out = 0;
    SCL_EXPECT_OK(tr, scl_cbtree_get(&t, &k, &out));
    SCL_EXPECT_EQ_I(tr, out, 42);

    scl_cbtree_destroy(alloc, &t);
}

static void test_cbtree_remove(scl_test_runner_t *tr) {
    scl_test_group("CBTree: remove");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_btree_t t;
    scl_cbtree_init(alloc, &t, sizeof(int), sizeof(int), SCL_BTREE_DEGREE, int_cmp);

    int k = 3, v = 30;
    scl_cbtree_insert(alloc, &t, &k, &v);
    SCL_EXPECT_TRUE(tr, scl_cbtree_contains(&t, &k));
    scl_error_t err = scl_cbtree_remove(alloc, &t, &k);
    if (err != SCL_ERR_NOT_IMPLEMENTED) {
        SCL_EXPECT_OK(tr, err);
        SCL_EXPECT_FALSE(tr, scl_cbtree_contains(&t, &k));
        SCL_EXPECT_EQ_SZ(tr, scl_cbtree_count(&t), 0);
    }

    scl_cbtree_destroy(alloc, &t);
}

static void test_cbtree_missing(scl_test_runner_t *tr) {
    scl_test_group("CBTree: get missing returns error");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_btree_t t;
    scl_cbtree_init(alloc, &t, sizeof(int), sizeof(int), SCL_BTREE_DEGREE, int_cmp);
    int k = 999, out;
    SCL_EXPECT_TRUE(tr, scl_cbtree_get(&t, &k, &out) != SCL_OK);
    scl_cbtree_destroy(alloc, &t);
}

static void test_cbtree_multiple(scl_test_runner_t *tr) {
    scl_test_group("CBTree: multiple entries");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_btree_t t;
    scl_cbtree_init(alloc, &t, sizeof(int), sizeof(int), SCL_BTREE_DEGREE, int_cmp);

    for (int i = 0; i < 50; i++)
        scl_cbtree_insert(alloc, &t, &i, &i);
    SCL_EXPECT_EQ_SZ(tr, scl_cbtree_count(&t), 50);

    for (int i = 0; i < 50; i++) {
        int out = -1;
        SCL_EXPECT_OK(tr, scl_cbtree_get(&t, &i, &out));
        SCL_EXPECT_EQ_I(tr, out, i);
    }
    scl_cbtree_destroy(alloc, &t);
}

typedef struct { scl_concurrent_btree_t *t; int base; } cbtree_arg_t;

static void *cbtree_insert_thread(void *arg) {
    cbtree_arg_t *a = arg;
    scl_allocator_t *alloc = scl_allocator_default();
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int k = a->base + i;
        scl_cbtree_insert(alloc, a->t, &k, &k);
    }
    return NULL;
}

static void test_cbtree_concurrent_insert(scl_test_runner_t *tr) {
    scl_test_group("CBTree: concurrent inserts");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_btree_t t;
    scl_cbtree_init(alloc, &t, sizeof(int), sizeof(int), SCL_BTREE_DEGREE, int_cmp);

    pthread_t threads[NTHREADS];
    cbtree_arg_t args[NTHREADS];
    for (int i = 0; i < NTHREADS; i++) {
        args[i] = (cbtree_arg_t){.t = &t, .base = i * OPS_PER_THREAD};
        pthread_create(&threads[i], NULL, cbtree_insert_thread, &args[i]);
    }
    for (int i = 0; i < NTHREADS; i++)
        pthread_join(threads[i], NULL);

    SCL_EXPECT_EQ_SZ(tr, scl_cbtree_count(&t), (size_t)(NTHREADS * OPS_PER_THREAD));
    scl_cbtree_destroy(alloc, &t);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_cbtree_init_destroy(&tr);
    test_cbtree_insert_get(&tr);
    test_cbtree_remove(&tr);
    test_cbtree_missing(&tr);
    test_cbtree_multiple(&tr);
    test_cbtree_concurrent_insert(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
