#include "scl_search_meta_binary_search.h"
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
    printf("=== scl_search_meta_binary_search tests ===\n");

    {
        int32_t arr[] = {1, 3, 5, 7, 9, 11, 13, 15};
        size_t idx;
        TEST("find existing");
        if (SCL_OK == scl_search_meta_binary_search(arr, 8, 7, &idx) && idx == 3) PASS();
        else FAIL("expected idx=3");
    }
    {
        int32_t arr[] = {1, 3, 5, 7, 9, 11, 13, 15};
        size_t idx;
        TEST("find first");
        if (SCL_OK == scl_search_meta_binary_search(arr, 8, 1, &idx) && idx == 0) PASS();
        else FAIL("expected idx=0");
    }
    {
        int32_t arr[] = {1, 3, 5, 7, 9, 11, 13, 15};
        size_t idx;
        TEST("find last");
        if (SCL_OK == scl_search_meta_binary_search(arr, 8, 15, &idx) && idx == 7) PASS();
        else FAIL("expected idx=7");
    }
    {
        int32_t arr[] = {1, 3, 5, 7, 9};
        size_t idx;
        TEST("not found");
        if (SCL_ERR_NOT_FOUND == scl_search_meta_binary_search(arr, 5, 4, &idx)) PASS();
        else FAIL("expected NOT_FOUND");
    }
    {
        size_t idx;
        TEST("null");
        if (SCL_ERR_NULL_PTR == scl_search_meta_binary_search(NULL, 0, 1, &idx)) PASS();
        else FAIL("expected NULL_PTR");
    }
    {
        int32_t arr[] = {42};
        size_t idx;
        TEST("single found");
        if (SCL_OK == scl_search_meta_binary_search(arr, 1, 42, &idx) && idx == 0) PASS();
        else FAIL("expected idx=0");
    }
    {
        int32_t arr[] = {42};
        size_t idx;
        TEST("single not found");
        if (SCL_ERR_NOT_FOUND == scl_search_meta_binary_search(arr, 1, 1, &idx)) PASS();
        else FAIL("expected NOT_FOUND");
    }
    {
        size_t idx;
        TEST("empty");
        if (SCL_ERR_EMPTY == scl_search_meta_binary_search((void*)(uintptr_t)1, 0, 5, &idx)) PASS();
        else FAIL("expected EMPTY");
    }
    {
        int32_t arr[] = {2, 4, 6, 8, 10, 12, 14, 16};
        size_t idx;
        TEST("find middle");
        if (SCL_OK == scl_search_meta_binary_search(arr, 8, 10, &idx) && idx == 4) PASS();
        else FAIL("expected idx=4");
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
