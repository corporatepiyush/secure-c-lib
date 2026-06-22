#include "scl_lru.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static void test_put_get(void)
{
    TEST("put and get");
    scl_lru_t cache;
    scl_lru_init(&cache, sizeof(int), sizeof(int), 3);
    int k, v;
    k = 1; v = 100; scl_lru_put(&cache, &k, &v);
    k = 2; v = 200; scl_lru_put(&cache, &k, &v);
    k = 3; v = 300; scl_lru_put(&cache, &k, &v);
    k = 1; assert(scl_lru_get(&cache, &k, &v) == SCL_OK && v == 100);
    k = 4; v = 400; scl_lru_put(&cache, &k, &v);
    k = 2; assert(scl_lru_get(&cache, &k, &v) == SCL_ERR_NOT_FOUND);
    scl_lru_destroy(&cache);
    PASS();
}

int main(void)
{
    printf("=== scl_lru tests ===\n");
    test_put_get();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
