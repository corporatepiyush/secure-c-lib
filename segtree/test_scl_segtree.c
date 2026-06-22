#include "scl_segtree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static int64_t sum_op(int64_t a, int64_t b) { return a + b; }

static void test_range_sum(void)
{
    TEST("range sum");
    int64_t data[] = {1, 2, 3, 4, 5, 6, 7, 8};
    scl_segtree_t st;
    scl_segtree_init(&st, data, 8, sum_op, 0);
    int64_t out;
    scl_segtree_query(&st, 0, 3, &out); assert(out == 10);
    scl_segtree_query(&st, 2, 5, &out); assert(out == 18);
    scl_segtree_update(&st, 3, 10);
    scl_segtree_query(&st, 0, 3, &out); assert(out == 16);
    scl_segtree_destroy(&st);
    PASS();
}

int main(void)
{
    printf("=== scl_segtree tests ===\n");
    test_range_sum();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
