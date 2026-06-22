#include "scl_search_kmp.h"
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
    printf("=== scl_search_kmp tests ===\n");

    {
        size_t pos;
        TEST("basic match");
        if (SCL_OK == scl_search_kmp("hello world", 11, "world", 5, &pos) && pos == 6) PASS();
        else FAIL("expected pos=6");
    }
    {
        size_t pos;
        TEST("match at start");
        if (SCL_OK == scl_search_kmp("hello world", 11, "hello", 5, &pos) && pos == 0) PASS();
        else FAIL("expected pos=0");
    }
    {
        size_t pos;
        TEST("match at end");
        if (SCL_OK == scl_search_kmp("hello world", 11, "world", 5, &pos) && pos == 6) PASS();
        else FAIL("expected pos=6");
    }
    {
        size_t pos;
        TEST("no match");
        if (SCL_ERR_NOT_FOUND == scl_search_kmp("hello world", 11, "xyz", 3, &pos)) PASS();
        else FAIL("expected NOT_FOUND");
    }
    {
        size_t pos;
        TEST("single char match");
        if (SCL_OK == scl_search_kmp("abc", 3, "b", 1, &pos) && pos == 1) PASS();
        else FAIL("expected pos=1");
    }
    {
        size_t pos;
        TEST("single char no match");
        if (SCL_ERR_NOT_FOUND == scl_search_kmp("abc", 3, "d", 1, &pos)) PASS();
        else FAIL("expected NOT_FOUND");
    }
    {
        size_t pos;
        TEST("pattern longer than text");
        if (SCL_ERR_NOT_FOUND == scl_search_kmp("ab", 2, "abc", 3, &pos)) PASS();
        else FAIL("expected NOT_FOUND");
    }
    {
        size_t pos;
        TEST("empty text");
        if (SCL_ERR_EMPTY == scl_search_kmp("", 0, "a", 1, &pos)) PASS();
        else FAIL("expected EMPTY");
    }
    {
        size_t pos;
        TEST("null text");
        if (SCL_ERR_NULL_PTR == scl_search_kmp(NULL, 0, "a", 1, &pos)) PASS();
        else FAIL("expected NULL_PTR");
    }
    {
        size_t pos;
        TEST("overlapping pattern");
        if (SCL_OK == scl_search_kmp("aaaaa", 5, "aaa", 3, &pos) && pos == 0) PASS();
        else FAIL("expected pos=0");
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
