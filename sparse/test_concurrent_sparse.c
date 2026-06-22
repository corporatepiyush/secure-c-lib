#include "concurrent_sparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static int64_t min_op(int64_t a, int64_t b) { return a < b ? a : b; }
static int64_t sum_op(int64_t a, int64_t b) { return a + b; }

static void test_init_destroy(void)
{
    TEST("init and destroy");
    int64_t data[] = {4, 2, 8, 1, 9, 3};
    scl_concurrent_sparse_t st;
    assert(scl_concurrent_sparse_init(&st, data, 6, min_op) == SCL_OK);
    scl_concurrent_sparse_destroy(&st);
    PASS();
}

static void test_query(void)
{
    TEST("query range");
    int64_t data[] = {4, 2, 8, 1, 9, 3};
    scl_concurrent_sparse_t st;
    scl_concurrent_sparse_init(&st, data, 6, min_op);
    int64_t out;
    assert(scl_concurrent_sparse_query(&st, 0, 5, &out) == SCL_OK && out == 1);
    assert(scl_concurrent_sparse_query(&st, 0, 2, &out) == SCL_OK && out == 2);
    assert(scl_concurrent_sparse_query(&st, 3, 5, &out) == SCL_OK && out == 1);
    assert(scl_concurrent_sparse_query(&st, 4, 4, &out) == SCL_OK && out == 9);
    scl_concurrent_sparse_destroy(&st);
    PASS();
}

static void test_sum(void)
{
    TEST("range sum");
    int64_t data[] = {1, 2, 3, 4, 5};
    scl_concurrent_sparse_t st;
    scl_concurrent_sparse_init(&st, data, 5, sum_op);
    int64_t out;
    assert(scl_concurrent_sparse_query(&st, 0, 4, &out) == SCL_OK && out == 15);
    assert(scl_concurrent_sparse_query(&st, 1, 3, &out) == SCL_OK && out == 9);
    scl_concurrent_sparse_destroy(&st);
    PASS();
}

static void test_invalid(void)
{
    TEST("invalid args");
    int64_t data[] = {1, 2, 3};
    scl_concurrent_sparse_t st;
    scl_concurrent_sparse_init(&st, data, 3, min_op);
    int64_t out;
    assert(scl_concurrent_sparse_query(&st, 2, 1, &out) == SCL_ERR_INVALID_INDEX);
    assert(scl_concurrent_sparse_query(&st, 5, 6, &out) == SCL_ERR_INVALID_INDEX);
    scl_concurrent_sparse_destroy(&st);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_concurrent_sparse_init(NULL, NULL, 5, min_op) == SCL_ERR_NULL_PTR);
    scl_concurrent_sparse_destroy(NULL);
    PASS();
}

int main(void)
{
    printf("=== scl_concurrent_sparse tests ===\n");
    test_init_destroy();
    test_query();
    test_sum();
    test_invalid();
    test_null();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
