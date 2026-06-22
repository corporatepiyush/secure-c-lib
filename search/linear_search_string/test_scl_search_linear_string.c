#include "scl_search_linear_string.h"
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
    printf("=== scl_search_linear_string tests ===\n");

    {
        const char *strs[] = {"apple", "banana", "cherry", "date"};
        size_t idx;
        TEST("find existing");
        if (SCL_OK == scl_search_linear_string(strs, 4, "cherry", &idx) && idx == 2) PASS();
        else FAIL("expected idx=2");
    }
    {
        const char *strs[] = {"apple", "banana", "cherry", "date"};
        size_t idx;
        TEST("find first");
        if (SCL_OK == scl_search_linear_string(strs, 4, "apple", &idx) && idx == 0) PASS();
        else FAIL("expected idx=0");
    }
    {
        const char *strs[] = {"apple", "banana", "cherry", "date"};
        size_t idx;
        TEST("find last");
        if (SCL_OK == scl_search_linear_string(strs, 4, "date", &idx) && idx == 3) PASS();
        else FAIL("expected idx=3");
    }
    {
        const char *strs[] = {"apple", "banana", "cherry", "date"};
        size_t idx;
        TEST("not found");
        if (SCL_ERR_NOT_FOUND == scl_search_linear_string(strs, 4, "elderberry", &idx)) PASS();
        else FAIL("expected NOT_FOUND");
    }
    {
        size_t idx;
        TEST("null strs");
        if (SCL_ERR_NULL_PTR == scl_search_linear_string(NULL, 0, "x", &idx)) PASS();
        else FAIL("expected NULL_PTR");
    }
    {
        const char *strs[] = {"only"};
        size_t idx;
        TEST("single found");
        if (SCL_OK == scl_search_linear_string(strs, 1, "only", &idx) && idx == 0) PASS();
        else FAIL("expected idx=0");
    }
    {
        const char *strs[] = {"only"};
        size_t idx;
        TEST("single not found");
        if (SCL_ERR_NOT_FOUND == scl_search_linear_string(strs, 1, "other", &idx)) PASS();
        else FAIL("expected NOT_FOUND");
    }
    {
        const char *strs[] = {"a", "b", "a", "c"};
        size_t idx;
        TEST("duplicates first");
        if (SCL_OK == scl_search_linear_string(strs, 4, "a", &idx) && idx == 0) PASS();
        else FAIL("expected idx=0");
    }
    {
        size_t idx;
        TEST("empty array");
        if (SCL_ERR_EMPTY == scl_search_linear_string((void*)(uintptr_t)1, 0, "x", &idx)) PASS();
        else FAIL("expected EMPTY");
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
