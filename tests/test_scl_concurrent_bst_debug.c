#include "scl_test.h"
#include "scl_concurrent_bst.h"
#include <pthread.h>
#include <stdio.h>

static int int_cmp(const void *a, const void *b) {
    int va = *(int *)a, vb = *(int *)b;
    return (va > vb) - (va < vb);
}

typedef struct { scl_concurrent_bst_t *t; int base; } debug_arg_t;

static void *debug_insert(void *arg) {
    debug_arg_t *a = arg;
    scl_allocator_t *alloc = scl_allocator_default();
    for (int i = 0; i < 200; i++) {
        int k = a->base + i;
        scl_cbst_insert(alloc, a->t, &k);
    }
    return NULL;
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    scl_test_group("CBST: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_bst_t t;

    scl_error_t err = scl_cbst_init(alloc, &t, sizeof(int), int_cmp);
    SCL_EXPECT_OK(&tr, err);
    SCL_EXPECT_EQ_SZ(&tr, scl_cbst_count(&t), 0);
    SCL_EXPECT_TRUE(&tr, scl_cbst_empty(&t));
    scl_cbst_destroy(alloc, &t);

    scl_test_group("CBST: insert and find");
    scl_cbst_init(alloc, &t, sizeof(int), int_cmp);
    int v = 42;
    SCL_EXPECT_OK(&tr, scl_cbst_insert(alloc, &t, &v));
    SCL_EXPECT_EQ_SZ(&tr, scl_cbst_count(&t), 1);
    SCL_EXPECT_TRUE(&tr, scl_cbst_contains(&t, &v));
    int out = 0;
    SCL_EXPECT_OK(&tr, scl_cbst_find(&t, &v, &out));
    SCL_EXPECT_EQ_I(&tr, out, 42);
    scl_cbst_destroy(alloc, &t);

    scl_test_group("CBST: remove");
    scl_cbst_init(alloc, &t, sizeof(int), int_cmp);
    v = 7;
    SCL_EXPECT_OK(&tr, scl_cbst_insert(alloc, &t, &v));
    SCL_EXPECT_TRUE(&tr, scl_cbst_contains(&t, &v));
    SCL_EXPECT_OK(&tr, scl_cbst_remove(alloc, &t, &v));
    SCL_EXPECT_FALSE(&tr, scl_cbst_contains(&t, &v));
    SCL_EXPECT_EQ_SZ(&tr, scl_cbst_count(&t), 0);
    scl_cbst_destroy(alloc, &t);

    scl_test_group("CBST: find missing returns error");
    scl_cbst_init(alloc, &t, sizeof(int), int_cmp);
    int key = 999;
    SCL_EXPECT_TRUE(&tr, scl_cbst_find(&t, &key, &out) != SCL_OK);
    scl_cbst_destroy(alloc, &t);

    scl_test_group("CBST: multiple entries");
    scl_cbst_init(alloc, &t, sizeof(int), int_cmp);
    for (int i = 0; i < 50; i++)
        (void)scl_cbst_insert(alloc, &t, &i);
    SCL_EXPECT_EQ_SZ(&tr, scl_cbst_count(&t), 50);
    for (int i = 0; i < 50; i++) {
        out = -1;
        SCL_EXPECT_OK(&tr, scl_cbst_find(&t, &i, &out));
        SCL_EXPECT_EQ_I(&tr, out, i);
    }
    scl_cbst_destroy(alloc, &t);

    scl_test_summary(&tr);
    printf("Sequential tests done, now concurrent...\n");
    fflush(stdout);

    pthread_t threads[4];
    scl_concurrent_bst_t ct;
    scl_cbst_init(alloc, &ct, sizeof(int), int_cmp);
    debug_arg_t args[4];
    for (int i = 0; i < 4; i++) {
        args[i] = (debug_arg_t){.t = &ct, .base = i * 500};
        pthread_create(&threads[i], NULL, debug_insert, &args[i]);
    }
    for (int i = 0; i < 4; i++)
        pthread_join(threads[i], NULL);
    printf("All threads joined.\n");
    scl_cbst_destroy(alloc, &ct);
    printf("Done.\n");
    return 0;
}
