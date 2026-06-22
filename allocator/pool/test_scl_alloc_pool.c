#include "scl_alloc_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

int main(void) {
    printf("=== scl_alloc_pool tests ===\n");

    TEST("init and destroy");
    {
        scl_alloc_pool_t pool;
        scl_error_t e = scl_alloc_pool_init(&pool, 64, 100);
        if (e == SCL_OK) { PASS(); } else { FAIL("init failed"); }
        scl_alloc_pool_destroy(&pool);
    }

    TEST("alloc and free one block");
    {
        scl_alloc_pool_t pool;
        scl_alloc_pool_init(&pool, 32, 10);
        void *p = NULL;
        scl_error_t e = scl_alloc_pool_alloc(&pool, &p);
        if (e == SCL_OK && p) {
            memset(p, 0xAB, 32);
            e = scl_alloc_pool_free(&pool, p);
            if (e == SCL_OK) { PASS(); } else { FAIL("free failed"); }
        } else { FAIL("alloc failed"); }
        scl_alloc_pool_destroy(&pool);
    }

    TEST("NULL pool returns SCL_ERR_NULL_PTR");
    {
        if (scl_alloc_pool_init(NULL, 64, 10) == SCL_ERR_NULL_PTR) { PASS(); }
        else { FAIL("expected NULL_PTR"); }
    }

    TEST("NULL out_ptr returns SCL_ERR_NULL_PTR");
    {
        scl_alloc_pool_t pool;
        scl_alloc_pool_init(&pool, 64, 10);
        if (scl_alloc_pool_alloc(&pool, NULL) == SCL_ERR_NULL_PTR) { PASS(); }
        else { FAIL("expected NULL_PTR"); }
        scl_alloc_pool_destroy(&pool);
    }

    TEST("exhaustion returns SCL_ERR_OUT_OF_MEMORY");
    {
        scl_alloc_pool_t pool;
        scl_alloc_pool_init(&pool, 16, 3);
        void *a, *b, *c, *d;
        assert(scl_alloc_pool_alloc(&pool, &a) == SCL_OK);
        assert(scl_alloc_pool_alloc(&pool, &b) == SCL_OK);
        assert(scl_alloc_pool_alloc(&pool, &c) == SCL_OK);
        if (scl_alloc_pool_alloc(&pool, &d) == SCL_ERR_OUT_OF_MEMORY) { PASS(); }
        else { FAIL("expected OOM"); }
        scl_alloc_pool_free(&pool, a);
        scl_alloc_pool_free(&pool, b);
        scl_alloc_pool_free(&pool, c);
        scl_alloc_pool_destroy(&pool);
    }

    TEST("stress 1000 allocs/frees");
    {
        scl_alloc_pool_t pool;
        scl_alloc_pool_init(&pool, 128, 1000);
        void *ptrs[1000];
        int ok = 1;
        for (int i = 0; i < 1000; i++) {
            if (scl_alloc_pool_alloc(&pool, &ptrs[i]) != SCL_OK) { ok = 0; break; }
            memset(ptrs[i], i & 0xFF, 128);
        }
        for (int i = 0; i < 1000; i++)
            scl_alloc_pool_free(&pool, ptrs[i]);
        if (ok) { PASS(); } else { FAIL("stress failed"); }
        scl_alloc_pool_destroy(&pool);
    }

    TEST("invalid free ptr");
    {
        scl_alloc_pool_t pool;
        scl_alloc_pool_init(&pool, 32, 5);
        int dummy;
        if (scl_alloc_pool_free(&pool, &dummy) == SCL_ERR_INVALID_ARG) { PASS(); }
        else { FAIL("expected INVALID_ARG"); }
        scl_alloc_pool_destroy(&pool);
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
