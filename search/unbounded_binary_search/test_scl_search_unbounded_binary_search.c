#include "scl_search_unbounded_binary_search.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static int arr[] = {2, 4, 6, 8, 10, 12, 14, 16, 18, 20};
static size_t arr_len = 10;

static void *getter(size_t i, void *ctx SCL_UNUSED)
{
    if (i >= arr_len) return NULL;
    return &arr[i];
}

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

int main(void)
{
    printf("=== scl_search_unbounded_binary_search tests ===\n");

    {
        size_t idx;
        TEST("find existing");
        if (SCL_OK == scl_search_unbounded_binary_search(cmp_int, &(int){8}, &idx, getter, NULL, arr_len) && idx == 3) PASS();
        else FAIL("expected idx=3");
    }
    {
        size_t idx;
        TEST("find first");
        if (SCL_OK == scl_search_unbounded_binary_search(cmp_int, &(int){2}, &idx, getter, NULL, arr_len) && idx == 0) PASS();
        else FAIL("expected idx=0");
    }
    {
        size_t idx;
        TEST("find last");
        if (SCL_OK == scl_search_unbounded_binary_search(cmp_int, &(int){20}, &idx, getter, NULL, arr_len) && idx == 9) PASS();
        else FAIL("expected idx=9");
    }
    {
        size_t idx;
        TEST("not found");
        if (SCL_ERR_NOT_FOUND == scl_search_unbounded_binary_search(cmp_int, &(int){5}, &idx, getter, NULL, arr_len)) PASS();
        else FAIL("expected NOT_FOUND");
    }
    {
        size_t idx;
        TEST("null cmp");
        if (SCL_ERR_NULL_PTR == scl_search_unbounded_binary_search(NULL, &(int){1}, &idx, getter, NULL, 5)) PASS();
        else FAIL("expected NULL_PTR");
    }
    {
        size_t idx;
        TEST("empty max_count");
        if (SCL_ERR_EMPTY == scl_search_unbounded_binary_search(cmp_int, &(int){1}, &idx, getter, NULL, 0)) PASS();
        else FAIL("expected EMPTY");
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
