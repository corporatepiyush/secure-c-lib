#include "scl_sparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static int64_t min_op(int64_t a, int64_t b) { return a < b ? a : b; }

static void test_range_min(void)
{
    TEST("range min query");
    int64_t data[] = {5, 2, 8, 1, 9, 3, 7, 4};
    scl_sparse_t st;
    scl_sparse_init(&st, data, 8, min_op);
    int64_t out;
    scl_sparse_query(&st, 0, 7, &out); assert(out == 1);
    scl_sparse_query(&st, 0, 0, &out); assert(out == 5);
    scl_sparse_query(&st, 4, 6, &out); assert(out == 3);
    scl_sparse_destroy(&st);
    PASS();
}

int main(void)
{
    printf("=== scl_sparse tests ===\n");
    test_range_min();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
