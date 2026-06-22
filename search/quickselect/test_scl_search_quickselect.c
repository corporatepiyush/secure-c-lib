#include "scl_search_quickselect.h"
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
    printf("=== scl_search_quickselect tests ===\n");

    {
        int arr[] = {9, 5, 7, 1, 3, 8, 2, 6, 4};
        int out;
        TEST("k=0 (min)");
        if (SCL_OK == scl_search_quickselect(arr, 9, sizeof(int), cmp_int, 0, &out) && out == 1) PASS();
        else FAIL("expected 1");
    }
    {
        int arr[] = {9, 5, 7, 1, 3, 8, 2, 6, 4};
        int out;
        TEST("k=4 (median)");
        if (SCL_OK == scl_search_quickselect(arr, 9, sizeof(int), cmp_int, 4, &out) && out == 5) PASS();
        else FAIL("expected 5");
    }
    {
        int arr[] = {9, 5, 7, 1, 3, 8, 2, 6, 4};
        int out;
        TEST("k=8 (max)");
        if (SCL_OK == scl_search_quickselect(arr, 9, sizeof(int), cmp_int, 8, &out) && out == 9) PASS();
        else FAIL("expected 9");
    }
    {
        int arr[] = {42};
        int out;
        TEST("single element");
        if (SCL_OK == scl_search_quickselect(arr, 1, sizeof(int), cmp_int, 0, &out) && out == 42) PASS();
        else FAIL("expected 42");
    }
    {
        int arr[] = {5, 3, 3, 1, 3};
        int out;
        TEST("duplicates");
        if (SCL_OK == scl_search_quickselect(arr, 5, sizeof(int), cmp_int, 2, &out) && out == 3) PASS();
        else FAIL("expected 3");
    }
    {
        int arr[] = {10, 20, 30};
        int out;
        TEST("k=1 middle");
        if (SCL_OK == scl_search_quickselect(arr, 3, sizeof(int), cmp_int, 1, &out) && out == 20) PASS();
        else FAIL("expected 20");
    }
    {
        size_t idx;
        TEST("null base");
        if (SCL_ERR_NULL_PTR == scl_search_quickselect(NULL, 1, sizeof(int), cmp_int, 0, &idx)) PASS();
        else FAIL("expected NULL_PTR");
    }
    {
        int arr[] = {1, 2, 3};
        int out;
        TEST("k out of range");
        if (SCL_ERR_INVALID_INDEX == scl_search_quickselect(arr, 3, sizeof(int), cmp_int, 5, &out)) PASS();
        else FAIL("expected INVALID_INDEX");
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
