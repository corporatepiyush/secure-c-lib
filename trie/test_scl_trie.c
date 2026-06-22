#include "scl_trie.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static void test_insert_search(void)
{
    TEST("insert and search");
    scl_trie_t t;
    scl_trie_init(&t, sizeof(int));
    int v = 1;
    assert(scl_trie_insert(&t, (const unsigned char *)"hello", 5, &v) == SCL_OK);
    assert(scl_trie_insert(&t, (const unsigned char *)"world", 5, &v) == SCL_OK);
    assert(scl_trie_insert(&t, (const unsigned char *)"hi", 2, &v) == SCL_OK);
    assert(scl_trie_contains(&t, (const unsigned char *)"hello", 5));
    int out; assert(scl_trie_get(&t, (const unsigned char *)"hello", 5, &out) == SCL_OK);
    assert(!scl_trie_contains(&t, (const unsigned char *)"notfound", 8));
    assert(scl_trie_count(&t) == 3);
    scl_trie_destroy(&t);
    PASS();
}

static void test_remove(void)
{
    TEST("remove");
    scl_trie_t t;
    scl_trie_init(&t, sizeof(int));
    int v = 1;
    scl_trie_insert(&t, (const unsigned char *)"test", 4, &v);
    assert(scl_trie_contains(&t, (const unsigned char *)"test", 4));
    scl_trie_remove(&t, (const unsigned char *)"test", 4);
    assert(!scl_trie_contains(&t, (const unsigned char *)"test", 4));
    scl_trie_destroy(&t);
    PASS();
}

int main(void)
{
    printf("=== scl_trie tests ===\n");
    test_insert_search();
    test_remove();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
