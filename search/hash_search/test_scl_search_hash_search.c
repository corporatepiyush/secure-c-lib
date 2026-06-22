#include "scl_search_hash_search.h"
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
    printf("=== scl_search_hash_search tests ===\n");

    {
        scl_search_ht_t *ht = NULL;
        TEST("init");
        if (SCL_OK == scl_search_ht_init(&ht, 16) && ht != NULL) { PASS(); scl_search_ht_destroy(ht); }
        else FAIL("init failed");
    }
    {
        scl_search_ht_t *ht = NULL;
        (void)scl_search_ht_init(&ht, 16);
        int v = 42;
        (void)scl_search_ht_insert(ht, "key1", &v);
        void *out;
        TEST("search found");
        if (SCL_OK == scl_search_ht_search(ht, "key1", &out) && *(int*)out == 42) PASS();
        else FAIL("expected 42");
        scl_search_ht_destroy(ht);
    }
    {
        scl_search_ht_t *ht = NULL;
        (void)scl_search_ht_init(&ht, 16);
        int v = 1;
        (void)scl_search_ht_insert(ht, "k", &v);
        void *out;
        TEST("search not found");
        if (SCL_ERR_NOT_FOUND == scl_search_ht_search(ht, "nope", &out)) PASS();
        else FAIL("expected NOT_FOUND");
        scl_search_ht_destroy(ht);
    }
    {
        scl_search_ht_t *ht = NULL;
        (void)scl_search_ht_init(&ht, 16);
        int v1 = 1, v2 = 2;
        (void)scl_search_ht_insert(ht, "a", &v1);
        (void)scl_search_ht_insert(ht, "b", &v2);
        TEST("search multiple");
        void *out;
        if (SCL_OK == scl_search_ht_search(ht, "b", &out) && *(int*)out == 2) PASS();
        else FAIL("expected 2");
        scl_search_ht_destroy(ht);
    }
    {
        scl_search_ht_t *ht = NULL;
        (void)scl_search_ht_init(&ht, 16);
        int v = 7;
        (void)scl_search_ht_insert(ht, "del", &v);
        TEST("delete");
        if (SCL_OK == scl_search_ht_delete(ht, "del")) {
            void *out;
            if (SCL_ERR_NOT_FOUND == scl_search_ht_search(ht, "del", &out)) PASS();
            else FAIL("should be gone");
        } else FAIL("delete failed");
        scl_search_ht_destroy(ht);
    }
    {
        scl_search_ht_t *ht = NULL;
        (void)scl_search_ht_init(&ht, 16);
        TEST("null ht search");
        if (SCL_ERR_NULL_PTR == scl_search_ht_search(NULL, "x", (void**)(uintptr_t)1)) PASS();
        else FAIL("expected NULL_PTR");
        scl_search_ht_destroy(ht);
    }
    {
        scl_search_ht_t *ht = NULL;
        (void)scl_search_ht_init(&ht, 16);
        int v = 10;
        (void)scl_search_ht_insert(ht, "update", &v);
        int v2 = 20;
        (void)scl_search_ht_insert(ht, "update", &v2);
        void *out;
        TEST("update value");
        if (SCL_OK == scl_search_ht_search(ht, "update", &out) && *(int*)out == 20) PASS();
        else FAIL("expected 20");
        scl_search_ht_destroy(ht);
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
