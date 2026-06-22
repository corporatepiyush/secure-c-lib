#include "scl_hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static void test_insert_get(void)
{
    TEST("insert and get");
    scl_hash_t ht;
    scl_hash_init(&ht, sizeof(int), sizeof(int), 16, scl_hash_djb2, NULL);
    for (int i = 0; i < 100; i++)
        assert(scl_hash_insert(&ht, &i, &i) == SCL_OK);
    assert(scl_hash_count(&ht) == 100);
    for (int i = 0; i < 100; i++) {
        int v; assert(scl_hash_get(&ht, &i, &v) == SCL_OK && v == i);
    }
    scl_hash_destroy(&ht);
    PASS();
}

static void test_remove_contains(void)
{
    TEST("remove and contains");
    scl_hash_t ht;
    scl_hash_init(&ht, sizeof(int), sizeof(int), 16, scl_hash_djb2, NULL);
    int k = 42, v = 99;
    scl_hash_insert(&ht, &k, &v);
    assert(scl_hash_contains(&ht, &k));
    scl_hash_remove(&ht, &k);
    assert(!scl_hash_contains(&ht, &k));
    assert(scl_hash_get(&ht, &k, &v) == SCL_ERR_NOT_FOUND);
    scl_hash_destroy(&ht);
    PASS();
}

int main(void)
{
    printf("=== scl_hash tests ===\n");
    test_insert_get();
    test_remove_contains();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
