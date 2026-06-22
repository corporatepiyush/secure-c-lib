#include "concurrent_btree.h"
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

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static void test_init_destroy(void)
{
    TEST("init and destroy");
    scl_concurrent_btree_t tree;
    assert(scl_concurrent_btree_init(&tree, sizeof(int), sizeof(int), 4, cmp_int) == SCL_OK);
    assert(scl_concurrent_btree_count(&tree) == 0);
    scl_concurrent_btree_destroy(&tree);
    PASS();
}

static void test_insert_get_contains(void)
{
    TEST("insert, get, contains");
    scl_concurrent_btree_t tree;
    scl_concurrent_btree_init(&tree, sizeof(int), sizeof(int), 4, cmp_int);
    int k, v;
    for (int i = 0; i < 50; i++) {
        k = i; v = i * 10;
        assert(scl_concurrent_btree_insert(&tree, &k, &v) == SCL_OK);
    }
    for (int i = 0; i < 50; i++) {
        int out;
        k = i;
        assert(scl_concurrent_btree_get(&tree, &k, &out) == SCL_OK);
        assert(out == i * 10);
        assert(scl_concurrent_btree_contains(&tree, &k));
    }
    k = 999;
    assert(scl_concurrent_btree_get(&tree, &k, &v) == SCL_ERR_NOT_FOUND);
    assert(!scl_concurrent_btree_contains(&tree, &k));
    scl_concurrent_btree_destroy(&tree);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_concurrent_btree_init(NULL, sizeof(int), sizeof(int), 4, cmp_int) == SCL_ERR_NULL_PTR);
    scl_concurrent_btree_destroy(NULL);
    PASS();
}

static void *insert_thread(void *arg)
{
    scl_concurrent_btree_t *tree = (scl_concurrent_btree_t *)arg;
    for (int i = 0; i < 25; i++) {
        int v = i;
        scl_concurrent_btree_insert(tree, &v, &v);
    }
    return NULL;
}

static void test_concurrent_insert(void)
{
    TEST("concurrent insert 2 threads");
    scl_concurrent_btree_t tree;
    scl_concurrent_btree_init(&tree, sizeof(int), sizeof(int), 4, cmp_int);
    pthread_t t1, t2;
    pthread_create(&t1, NULL, insert_thread, &tree);
    pthread_create(&t2, NULL, insert_thread, &tree);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    assert(scl_concurrent_btree_count(&tree) == 25);
    scl_concurrent_btree_destroy(&tree);
    PASS();
}

int main(void)
{
    printf("=== scl_concurrent_btree tests ===\n");
    test_init_destroy();
    test_insert_get_contains();
    test_null();
    test_concurrent_insert();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
