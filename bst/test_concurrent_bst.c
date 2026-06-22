#include "concurrent_bst.h"
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
    scl_concurrent_bst_t tree;
    assert(scl_concurrent_bst_init(&tree, sizeof(int), cmp_int) == SCL_OK);
    assert(scl_concurrent_bst_empty(&tree));
    scl_concurrent_bst_destroy(&tree);
    PASS();
}

static void test_insert_contains_find(void)
{
    TEST("insert, contains, find");
    scl_concurrent_bst_t tree;
    scl_concurrent_bst_init(&tree, sizeof(int), cmp_int);
    int vals[] = {50, 30, 70, 20, 40, 60, 80};
    for (int i = 0; i < 7; i++) assert(scl_concurrent_bst_insert(&tree, &vals[i]) == SCL_OK);
    assert(scl_concurrent_bst_count(&tree) == 7);
    for (int i = 0; i < 7; i++) assert(scl_concurrent_bst_contains(&tree, &vals[i]));
    int v;
    for (int i = 0; i < 7; i++) { assert(scl_concurrent_bst_find(&tree, &vals[i], &v) == SCL_OK); assert(v == vals[i]); }
    int nk = 99;
    assert(!scl_concurrent_bst_contains(&tree, &nk));
    assert(scl_concurrent_bst_find(&tree, &nk, &v) == SCL_ERR_NOT_FOUND);
    scl_concurrent_bst_destroy(&tree);
    PASS();
}

static void test_remove(void)
{
    TEST("remove");
    scl_concurrent_bst_t tree;
    scl_concurrent_bst_init(&tree, sizeof(int), cmp_int);
    for (int i = 0; i < 10; i++) scl_concurrent_bst_insert(&tree, &i);
    for (int i = 0; i < 10; i += 2) assert(scl_concurrent_bst_remove(&tree, &i) == SCL_OK);
    assert(scl_concurrent_bst_count(&tree) == 5);
    for (int i = 0; i < 10; i++) {
        if (i % 2 == 0) assert(!scl_concurrent_bst_contains(&tree, &i));
        else assert(scl_concurrent_bst_contains(&tree, &i));
    }
    scl_concurrent_bst_destroy(&tree);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_concurrent_bst_init(NULL, sizeof(int), cmp_int) == SCL_ERR_NULL_PTR);
    scl_concurrent_bst_destroy(NULL);
    PASS();
}

static void *insert_thread(void *arg)
{
    scl_concurrent_bst_t *tree = (scl_concurrent_bst_t *)arg;
    for (int i = 0; i < 50; i++) scl_concurrent_bst_insert(tree, &i);
    return NULL;
}

static void test_concurrent_insert(void)
{
    TEST("concurrent insert 2 threads");
    scl_concurrent_bst_t tree;
    scl_concurrent_bst_init(&tree, sizeof(int), cmp_int);
    pthread_t t1, t2;
    pthread_create(&t1, NULL, insert_thread, &tree);
    pthread_create(&t2, NULL, insert_thread, &tree);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    assert(scl_concurrent_bst_count(&tree) == 50);
    for (int i = 0; i < 50; i++) assert(scl_concurrent_bst_contains(&tree, &i));
    scl_concurrent_bst_destroy(&tree);
    PASS();
}

int main(void)
{
    printf("=== scl_concurrent_bst tests ===\n");
    test_init_destroy();
    test_insert_contains_find();
    test_remove();
    test_null();
    test_concurrent_insert();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
