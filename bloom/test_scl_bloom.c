#include "scl_bloom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static void test_insert_contains(void)
{
    TEST("insert and maybe contains");
    scl_bloom_t bf;
    scl_bloom_init(&bf, 100, 0.01, scl_bloom_hash_murmur);
    const char *key = "test_key";
    scl_bloom_insert(&bf, key, strlen(key));
    assert(scl_bloom_maybe_contains(&bf, key, strlen(key)));
    assert(!scl_bloom_maybe_contains(&bf, "not_inserted", 12));
    scl_bloom_destroy(&bf);
    PASS();
}

int main(void)
{
    printf("=== scl_bloom tests ===\n");
    test_insert_contains();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
