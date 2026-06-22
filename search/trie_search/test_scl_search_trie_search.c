#include "scl_search_trie_search.h"
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
    printf("=== scl_search_trie_search tests ===\n");

    {
        scl_search_trie_t *t = NULL;
        TEST("init");
        if (SCL_OK == scl_search_trie_init(&t) && t != NULL) { PASS(); scl_search_trie_destroy(t); }
        else FAIL("init failed");
    }
    {
        scl_search_trie_t *t = NULL;
        (void)scl_search_trie_init(&t);
        (void)scl_search_trie_insert(t, "hello");
        TEST("search found");
        if (scl_search_trie_search(t, "hello")) PASS();
        else FAIL("expected true");
        scl_search_trie_destroy(t);
    }
    {
        scl_search_trie_t *t = NULL;
        (void)scl_search_trie_init(&t);
        (void)scl_search_trie_insert(t, "hello");
        TEST("search not found");
        if (!scl_search_trie_search(t, "world")) PASS();
        else FAIL("expected false");
        scl_search_trie_destroy(t);
    }
    {
        scl_search_trie_t *t = NULL;
        (void)scl_search_trie_init(&t);
        (void)scl_search_trie_insert(t, "hello");
        TEST("starts_with");
        if (scl_search_trie_starts_with(t, "hel")) PASS();
        else FAIL("expected true");
        scl_search_trie_destroy(t);
    }
    {
        scl_search_trie_t *t = NULL;
        (void)scl_search_trie_init(&t);
        (void)scl_search_trie_insert(t, "hello");
        TEST("starts_with not");
        if (!scl_search_trie_starts_with(t, "xyz")) PASS();
        else FAIL("expected false");
        scl_search_trie_destroy(t);
    }
    {
        scl_search_trie_t *t = NULL;
        (void)scl_search_trie_init(&t);
        (void)scl_search_trie_insert(t, "hello");
        (void)scl_search_trie_insert(t, "help");
        (void)scl_search_trie_insert(t, "world");
        TEST("delete word");
        scl_search_trie_delete(t, "hello");
        if (!scl_search_trie_search(t, "hello") && scl_search_trie_search(t, "help") && scl_search_trie_search(t, "world")) PASS();
        else FAIL("delete failed");
        scl_search_trie_destroy(t);
    }
    {
        scl_search_trie_t *t = NULL;
        (void)scl_search_trie_init(&t);
        (void)scl_search_trie_insert(t, "");
        TEST("empty string insert");
        if (scl_search_trie_search(t, "")) PASS();
        else FAIL("expected true");
        scl_search_trie_destroy(t);
    }
    {
        scl_search_trie_t *t = NULL;
        (void)scl_search_trie_init(&t);
        TEST("null trie");
        if (!scl_search_trie_search(NULL, "a")) PASS();
        else FAIL("expected false");
        scl_search_trie_destroy(t);
    }
    {
        scl_search_trie_t *t = NULL;
        (void)scl_search_trie_init(&t);
        (void)scl_search_trie_insert(t, "a");
        TEST("single char");
        if (scl_search_trie_search(t, "a")) PASS();
        else FAIL("expected true");
        scl_search_trie_destroy(t);
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
