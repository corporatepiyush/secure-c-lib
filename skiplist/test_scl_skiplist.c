#include "scl_skiplist.h"
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

static void test_insert_find(void)
{
    TEST("insert and find");
    scl_skiplist_t sl;
    scl_skiplist_init(&sl, sizeof(int), cmp_int);
    int data[] = {5, 3, 8, 1, 9, 2, 7, 4, 6, 0};
    for (size_t i = 0; i < 10; i++) scl_skiplist_insert(&sl, &data[i]);
    assert(scl_skiplist_count(&sl) == 10);
    for (int i = 0; i < 10; i++) assert(scl_skiplist_contains(&sl, &i));
    scl_skiplist_destroy(&sl);
    PASS();
}

static void test_remove(void)
{
    TEST("remove");
    scl_skiplist_t sl;
    scl_skiplist_init(&sl, sizeof(int), cmp_int);
    for (int i = 0; i < 10; i++) scl_skiplist_insert(&sl, &i);
    scl_skiplist_remove(&sl, &(int){5});
    assert(!scl_skiplist_contains(&sl, &(int){5}));
    scl_skiplist_destroy(&sl);
    PASS();
}

int main(void)
{
    printf("=== scl_skiplist tests ===\n");
    test_insert_find();
    test_remove();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
