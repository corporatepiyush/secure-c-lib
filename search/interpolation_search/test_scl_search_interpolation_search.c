#include "scl_search_interpolation_search.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

int main(void)
{
    printf("=== scl_search_interpolation_search tests ===\n");

    {
        int64_t arr[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
        size_t idx;
        TEST("find existing");
        if (SCL_OK == scl_search_interpolation_search(arr, 10, 50, &idx) && idx == 4) PASS();
        else FAIL("expected idx=4");
    }
    {
        int64_t arr[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
        size_t idx;
        TEST("find first");
        if (SCL_OK == scl_search_interpolation_search(arr, 10, 10, &idx) && idx == 0) PASS();
        else FAIL("expected idx=0");
    }
    {
        int64_t arr[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
        size_t idx;
        TEST("find last");
        if (SCL_OK == scl_search_interpolation_search(arr, 10, 100, &idx) && idx == 9) PASS();
        else FAIL("expected idx=9");
    }
    {
        int64_t arr[] = {10, 20, 30, 40, 50};
        size_t idx;
        TEST("not found low");
        if (SCL_ERR_NOT_FOUND == scl_search_interpolation_search(arr, 5, 5, &idx)) PASS();
        else FAIL("expected NOT_FOUND");
    }
    {
        int64_t arr[] = {10, 20, 30, 40, 50};
        size_t idx;
        TEST("not found high");
        if (SCL_ERR_NOT_FOUND == scl_search_interpolation_search(arr, 5, 99, &idx)) PASS();
        else FAIL("expected NOT_FOUND");
    }
    {
        int64_t arr[] = {10, 20, 30, 40, 50};
        size_t idx;
        TEST("not found between");
        if (SCL_ERR_NOT_FOUND == scl_search_interpolation_search(arr, 5, 25, &idx)) PASS();
        else FAIL("expected NOT_FOUND");
    }
    {
        size_t idx;
        TEST("null arr");
        if (SCL_ERR_NULL_PTR == scl_search_interpolation_search(NULL, 0, 5, &idx)) PASS();
        else FAIL("expected NULL_PTR");
    }
    {
        int64_t arr[] = {42};
        size_t idx;
        TEST("single found");
        if (SCL_OK == scl_search_interpolation_search(arr, 1, 42, &idx) && idx == 0) PASS();
        else FAIL("expected idx=0");
    }
    {
        int64_t arr[] = {42};
        size_t idx;
        TEST("single not found");
        if (SCL_ERR_NOT_FOUND == scl_search_interpolation_search(arr, 1, 1, &idx)) PASS();
        else FAIL("expected NOT_FOUND");
    }
    {
        size_t idx;
        TEST("empty");
        if (SCL_ERR_EMPTY == scl_search_interpolation_search((void*)(uintptr_t)1, 0, 5, &idx)) PASS();
        else FAIL("expected EMPTY");
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
