#include "scl_fenwick.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static void test_prefix_range(void)
{
    TEST("prefix and range sum");
    int64_t data[] = {1, 2, 3, 4, 5};
    scl_fenwick_t ft;
    scl_fenwick_init(&ft, data, 5);
    int64_t out;
    scl_fenwick_prefix(&ft, 2, &out); assert(out == 6);
    scl_fenwick_range(&ft, 1, 3, &out); assert(out == 9);
    scl_fenwick_update(&ft, 2, 10);
    scl_fenwick_prefix(&ft, 4, &out); assert(out == 25);
    scl_fenwick_destroy(&ft);
    PASS();
}

int main(void)
{
    printf("=== scl_fenwick tests ===\n");
    test_prefix_range();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
