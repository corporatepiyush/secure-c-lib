#include "scl_search_a_star.h"
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
    printf("=== scl_search_a_star tests ===\n");

    {
        int row0[] = {0, 0, 0, 0, 0};
        int row1[] = {0, 1, 1, 1, 0};
        int row2[] = {0, 0, 0, 1, 0};
        int row3[] = {0, 1, 0, 0, 0};
        int row4[] = {0, 0, 0, 1, 0};
        int *grid[] = {row0, row1, row2, row3, row4};
        int px[100], py[100];
        size_t plen;
        TEST("A* finds path in 5x5");
        scl_error_t err = scl_search_a_star(0, 0, 4, 4, grid, 5, 5, px, py, &plen, 100);
        if (err == SCL_OK && plen > 0) PASS();
        else FAIL("expected path found");
    }
    {
        int row0[] = {0, 0, 0, 0, 0};
        int row1[] = {1, 1, 1, 1, 1};
        int row2[] = {0, 0, 0, 0, 0};
        int *grid[] = {row0, row1, row2};
        int px[100], py[100];
        size_t plen;
        TEST("A* no path through wall");
        scl_error_t err = scl_search_a_star(0, 0, 4, 2, grid, 5, 3, px, py, &plen, 100);
        if (err == SCL_ERR_NOT_FOUND) PASS();
        else FAIL("expected NOT_FOUND");
    }
    {
        int row0[] = {0, 0};
        int *grid[] = {row0};
        int px[100], py[100];
        size_t plen;
        TEST("A* trivial path");
        scl_error_t err = scl_search_a_star(0, 0, 1, 0, grid, 2, 1, px, py, &plen, 100);
        if (err == SCL_OK && plen > 0) PASS();
        else FAIL("expected path found");
    }
    {
        int row0[] = {0};
        int *grid[] = {row0};
        int px[100], py[100];
        size_t plen;
        TEST("A* single cell start=goal");
        scl_error_t err = scl_search_a_star(0, 0, 0, 0, grid, 1, 1, px, py, &plen, 100);
        if (err == SCL_OK && plen == 1) PASS();
        else FAIL("expected path of len 1");
    }
    {
        TEST("null grid");
        size_t plen;
        if (SCL_ERR_NULL_PTR == scl_search_a_star(0, 0, 1, 1, NULL, 2, 2, (int*)(uintptr_t)1, (int*)(uintptr_t)1, &plen, 100)) PASS();
        else FAIL("expected NULL_PTR");
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
