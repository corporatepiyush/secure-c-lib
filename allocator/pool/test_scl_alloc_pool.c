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

    TEST("create and destroy");
    {
        scl_allocator_t *pool = scl_alloc_pool_create(scl_allocator_default(), 64, 100, 16);
        if (pool) { PASS(); } else { FAIL("create failed"); }
        scl_alloc_pool_destroy(pool);
    }

    TEST("alloc and free one block");
    {
        scl_allocator_t *pool = scl_alloc_pool_create(scl_allocator_default(), 32, 10, 16);
        void *p = scl_alloc(pool, 32, 16);
        if (p) {
            memset(p, 0xAB, 32);
            scl_free(pool, p);
            PASS();
        } else { FAIL("alloc failed"); }
        scl_alloc_pool_destroy(pool);
    }

    TEST("exhaustion returns NULL");
    {
        scl_allocator_t *pool = scl_alloc_pool_create(scl_allocator_default(), 16, 3, 16);
        void *a = scl_alloc(pool, 16, 16);
        void *b = scl_alloc(pool, 16, 16);
        void *c = scl_alloc(pool, 16, 16);
        void *d = scl_alloc(pool, 16, 16);
        if (a && b && c && !d) { PASS(); }
        else { FAIL("expected 3 allocs then OOM"); }
        scl_free(pool, a);
        scl_free(pool, b);
        scl_free(pool, c);
        scl_alloc_pool_destroy(pool);
    }

    TEST("stress 1000 allocs/frees");
    {
        scl_allocator_t *pool = scl_alloc_pool_create(scl_allocator_default(), 128, 1000, 16);
        void *ptrs[1000];
        int ok = 1;
        for (int i = 0; i < 1000; i++) {
            ptrs[i] = scl_alloc(pool, 128, 16);
            if (!ptrs[i]) { ok = 0; break; }
            memset(ptrs[i], i & 0xFF, 128);
        }
        for (int i = 0; i < 1000; i++)
            scl_free(pool, ptrs[i]);
        if (ok) { PASS(); } else { FAIL("stress failed"); }
        scl_alloc_pool_destroy(pool);
    }

    TEST("NULL backing returns NULL");
    {
        scl_allocator_t *pool = scl_alloc_pool_create(NULL, 64, 10, 16);
        if (!pool) { PASS(); } else { FAIL("expected NULL"); }
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
