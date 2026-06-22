#include "scl_avl.h"
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

static void test_insert_balance(void)
{
    TEST("insert maintains balance");
    scl_avl_t t;
    scl_avl_init(&t, sizeof(int), cmp_int);
    for (int i = 0; i < 100; i++) scl_avl_insert(&t, &i);
    assert(scl_avl_count(&t) == 100);
    for (int i = 0; i < 100; i++) assert(scl_avl_contains(&t, &i));
    scl_avl_destroy(&t);
    PASS();
}

static void test_remove(void)
{
    TEST("remove");
    scl_avl_t t;
    scl_avl_init(&t, sizeof(int), cmp_int);
    for (int i = 0; i < 50; i++) scl_avl_insert(&t, &i);
    for (int i = 0; i < 50; i += 2) scl_avl_remove(&t, &i);
    assert(scl_avl_count(&t) == 25);
    scl_avl_destroy(&t);
    PASS();
}

int main(void)
{
    printf("=== scl_avl tests ===\n");
    test_insert_balance();
    test_remove();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
