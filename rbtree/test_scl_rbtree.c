#include "scl_rbtree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static void visit_fill(void *data, void *ctx)
{
    int *arr = (int *)ctx;
    arr[*(int *)data] = *(int *)data;
}

static void test_insert_contains(void)
{
    TEST("insert and contains");
    scl_rbtree_t t;
    scl_rbtree_init(&t, sizeof(int), cmp_int);
    for (int i = 0; i < 100; i++) scl_rbtree_insert(&t, &i);
    assert(scl_rbtree_count(&t) == 100);
    for (int i = 0; i < 100; i++) assert(scl_rbtree_contains(&t, &i));
    scl_rbtree_destroy(&t);
    PASS();
}

static void test_remove(void)
{
    TEST("remove");
    scl_rbtree_t t;
    scl_rbtree_init(&t, sizeof(int), cmp_int);
    for (int i = 0; i < 50; i++) scl_rbtree_insert(&t, &i);
    for (int i = 0; i < 50; i += 2) scl_rbtree_remove(&t, &i);
    assert(scl_rbtree_count(&t) == 25);
    scl_rbtree_destroy(&t);
    PASS();
}

static void test_inorder(void)
{
    TEST("inorder");
    scl_rbtree_t t;
    scl_rbtree_init(&t, sizeof(int), cmp_int);
    int data[] = {5, 3, 7, 2, 4, 6, 8};
    for (size_t i = 0; i < 7; i++) scl_rbtree_insert(&t, &data[i]);
    assert(scl_rbtree_count(&t) == 7);
    int sorted[7] = {0};
    scl_rbtree_inorder(&t, visit_fill, sorted);
    scl_rbtree_destroy(&t);
    PASS();
}

int main(void)
{
    printf("=== scl_rbtree tests ===\n");
    test_insert_contains();
    test_remove();
    test_inorder();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
