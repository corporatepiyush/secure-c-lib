#include "scl_heap.h"
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

static void test_min_heap(void)
{
    TEST("min-heap");
    scl_heap_t h;
    scl_heap_init(&h, sizeof(int), 0, cmp_int);
    int vals[] = {5, 3, 8, 1, 9, 2, 7, 4, 6, 0};
    for (size_t i = 0; i < 10; i++) scl_heap_push(&h, &vals[i]);
    assert(scl_heap_count(&h) == 10);
    for (int i = 0; i < 10; i++) {
        int v; scl_heap_pop(&h, &v); assert(v == i);
    }
    scl_heap_destroy(&h);
    PASS();
}

static void test_search(void)
{
    TEST("search");
    scl_heap_t h;
    scl_heap_init(&h, sizeof(int), 0, cmp_int);
    int vals[] = {5, 3, 8, 1, 9, 2, 7, 4, 6, 0};
    for (size_t i = 0; i < 10; i++) scl_heap_push(&h, &vals[i]);
    size_t idx;
    int key = 5;
    assert(scl_heap_search(&h, &key, cmp_int, &idx) == SCL_OK);
    key = 999;
    assert(scl_heap_search(&h, &key, cmp_int, &idx) == SCL_ERR_NOT_FOUND);
    scl_heap_destroy(&h);
    PASS();
}

int main(void)
{
    printf("=== scl_heap tests ===\n");
    test_min_heap();
    test_search();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
