#include "scl_bst.h"
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

static void test_insert_inorder(void)
{
    TEST("insert and inorder");
    scl_bst_t t;
    scl_bst_init(&t, sizeof(int), cmp_int);
    int data[] = {5, 3, 7, 2, 4, 6, 8};
    for (size_t i = 0; i < 7; i++) scl_bst_insert(&t, &data[i]);
    assert(scl_bst_count(&t) == 7);
    int sorted[7] = {0};
    scl_bst_inorder(&t, visit_fill, sorted);
    scl_bst_destroy(&t);
    PASS();
}

static void test_remove(void)
{
    TEST("remove");
    scl_bst_t t;
    scl_bst_init(&t, sizeof(int), cmp_int);
    for (int i = 0; i < 10; i++) scl_bst_insert(&t, &i);
    scl_bst_remove(&t, &(int){5});
    assert(!scl_bst_contains(&t, &(int){5}));
    assert(scl_bst_count(&t) == 9);
    scl_bst_destroy(&t);
    PASS();
}

int main(void)
{
    printf("=== scl_bst tests ===\n");
    test_insert_inorder();
    test_remove();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
