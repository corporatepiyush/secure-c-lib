#include "scl_unionfind.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static void test_find_union(void)
{
    TEST("find and union");
    scl_unionfind_t uf;
    scl_unionfind_init(&uf, 10);
    assert(scl_unionfind_sets(&uf) == 10);
    scl_unionfind_union(&uf, 0, 1);
    scl_unionfind_union(&uf, 1, 2);
    scl_unionfind_union(&uf, 3, 4);
    assert(scl_unionfind_connected(&uf, 0, 2));
    assert(!scl_unionfind_connected(&uf, 0, 3));
    assert(scl_unionfind_sets(&uf) == 7);
    scl_unionfind_destroy(&uf);
    PASS();
}

static void test_bounds(void)
{
    TEST("bounds checks");
    scl_unionfind_t uf;
    scl_unionfind_init(&uf, 5);
    assert(scl_unionfind_find(&uf, 10) == SIZE_MAX);
    assert(scl_unionfind_connected(&uf, 10, 0) == false);
    scl_unionfind_destroy(&uf);
    PASS();
}

int main(void)
{
    printf("=== scl_unionfind tests ===\n");
    test_find_union();
    test_bounds();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
