#include "scl_btree.h"
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

static void test_insert_get(void)
{
    TEST("insert and get");
    scl_btree_t t;
    scl_btree_init(&t, sizeof(int), sizeof(int), 4, cmp_int);
    for (int i = 0; i < 50; i++) assert(scl_btree_insert(&t, &i, &i) == SCL_OK);
    assert(scl_btree_count(&t) == 50);
    for (int i = 0; i < 50; i++) {
        int v; assert(scl_btree_get(&t, &i, &v) == SCL_OK && v == i);
    }
    scl_btree_destroy(&t);
    PASS();
}

int main(void)
{
    printf("=== scl_btree tests ===\n");
    test_insert_get();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
