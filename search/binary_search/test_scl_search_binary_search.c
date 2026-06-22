#include "scl_search_binary_search.h"
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
    printf("=== scl_search_binary_search tests ===\n");

    {
        int arr[] = {1, 3, 5, 7, 9, 11, 13};
        size_t idx;
        TEST("find existing middle");
        if (SCL_OK == scl_search_binary_search(arr, 7, sizeof(int), &(int){7}, cmp_int, &idx) && idx == 3) PASS();
        else FAIL("expected idx=3");
    }
    {
        int arr[] = {1, 3, 5, 7, 9, 11, 13};
        size_t idx;
        TEST("find first");
        if (SCL_OK == scl_search_binary_search(arr, 7, sizeof(int), &(int){1}, cmp_int, &idx) && idx == 0) PASS();
        else FAIL("expected idx=0");
    }
    {
        int arr[] = {1, 3, 5, 7, 9, 11, 13};
        size_t idx;
        TEST("find last");
        if (SCL_OK == scl_search_binary_search(arr, 7, sizeof(int), &(int){13}, cmp_int, &idx) && idx == 6) PASS();
        else FAIL("expected idx=6");
    }
    {
        int arr[] = {1, 3, 5, 7, 9, 11, 13};
        size_t idx;
        TEST("not found (low)");
        if (SCL_ERR_NOT_FOUND == scl_search_binary_search(arr, 7, sizeof(int), &(int){0}, cmp_int, &idx)) PASS();
        else FAIL("expected NOT_FOUND");
    }
    {
        int arr[] = {1, 3, 5, 7, 9, 11, 13};
        size_t idx;
        TEST("not found (high)");
        if (SCL_ERR_NOT_FOUND == scl_search_binary_search(arr, 7, sizeof(int), &(int){99}, cmp_int, &idx)) PASS();
        else FAIL("expected NOT_FOUND");
    }
    {
        size_t idx;
        TEST("null base");
        if (SCL_ERR_NULL_PTR == scl_search_binary_search(NULL, 0, sizeof(int), &(int){1}, cmp_int, &idx)) PASS();
        else FAIL("expected NULL_PTR");
    }
    {
        int arr[] = {42};
        size_t idx;
        TEST("single found");
        if (SCL_OK == scl_search_binary_search(arr, 1, sizeof(int), &(int){42}, cmp_int, &idx) && idx == 0) PASS();
        else FAIL("expected idx=0");
    }
    {
        int arr[] = {42};
        size_t idx;
        TEST("single not found");
        if (SCL_ERR_NOT_FOUND == scl_search_binary_search(arr, 1, sizeof(int), &(int){1}, cmp_int, &idx)) PASS();
        else FAIL("expected NOT_FOUND");
    }
    {
        size_t idx;
        TEST("empty");
        if (SCL_ERR_EMPTY == scl_search_binary_search((void*)(uintptr_t)1, 0, sizeof(int), &(int){1}, cmp_int, &idx)) PASS();
        else FAIL("expected EMPTY");
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
