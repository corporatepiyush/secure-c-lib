#include "scl_search_sentinel_linear_search.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

int main(void)
{
    printf("=== scl_search_sentinel_linear_search tests ===\n");

    {
        int arr[] = {5, 2, 8, 1, 9, 3};
        size_t idx;
        TEST("find existing");
        if (SCL_OK == scl_search_sentinel_linear_search(arr, 6, sizeof(int), &(int){8}, cmp_int, &idx) && idx == 2) PASS();
        else FAIL("expected idx=2");
    }
    {
        int arr[] = {5, 2, 8, 1, 9, 3};
        size_t idx;
        TEST("find first");
        if (SCL_OK == scl_search_sentinel_linear_search(arr, 6, sizeof(int), &(int){5}, cmp_int, &idx) && idx == 0) PASS();
        else FAIL("expected idx=0");
    }
    {
        int arr[] = {5, 2, 8, 1, 9, 3};
        size_t idx;
        TEST("find last");
        if (SCL_OK == scl_search_sentinel_linear_search(arr, 6, sizeof(int), &(int){3}, cmp_int, &idx) && idx == 5) PASS();
        else FAIL("expected idx=5");
    }
    {
        int arr[] = {5, 2, 8, 1, 9, 3};
        size_t idx;
        TEST("not found");
        if (SCL_ERR_NOT_FOUND == scl_search_sentinel_linear_search(arr, 6, sizeof(int), &(int){99}, cmp_int, &idx)) PASS();
        else FAIL("expected NOT_FOUND");
    }
    {
        size_t idx;
        TEST("null");
        if (SCL_ERR_NULL_PTR == scl_search_sentinel_linear_search(NULL, 0, sizeof(int), &(int){1}, cmp_int, &idx)) PASS();
        else FAIL("expected NULL_PTR");
    }
    {
        int arr[] = {42};
        size_t idx;
        TEST("single found");
        if (SCL_OK == scl_search_sentinel_linear_search(arr, 1, sizeof(int), &(int){42}, cmp_int, &idx) && idx == 0) PASS();
        else FAIL("expected idx=0");
    }
    {
        int arr[] = {42};
        size_t idx;
        TEST("single not found");
        if (SCL_ERR_NOT_FOUND == scl_search_sentinel_linear_search(arr, 1, sizeof(int), &(int){1}, cmp_int, &idx)) PASS();
        else FAIL("expected NOT_FOUND");
    }
    {
        size_t idx;
        TEST("empty");
        if (SCL_ERR_EMPTY == scl_search_sentinel_linear_search((void*)(uintptr_t)1, 0, sizeof(int), &(int){1}, cmp_int, &idx)) PASS();
        else FAIL("expected EMPTY");
    }
    {
        int arr[] = {1, 2, 2, 2, 3};
        size_t idx;
        TEST("duplicates first");
        if (SCL_OK == scl_search_sentinel_linear_search(arr, 5, sizeof(int), &(int){2}, cmp_int, &idx) && idx == 1) PASS();
        else FAIL("expected idx=1");
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
