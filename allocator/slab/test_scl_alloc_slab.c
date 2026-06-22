#include "scl_alloc_slab.h"
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
    printf("=== scl_alloc_slab tests ===\n");

    TEST("init and destroy");
    {
        scl_alloc_slab_t slab;
        scl_error_t e = scl_alloc_slab_init(&slab);
        if (e == SCL_OK) { PASS(); } else { FAIL("init failed"); }
        scl_alloc_slab_destroy(&slab);
    }

    TEST("alloc small (8 bytes)");
    {
        scl_alloc_slab_t slab;
        scl_alloc_slab_init(&slab);
        void *p = NULL;
        if (scl_alloc_slab_alloc(&slab, 8, &p) == SCL_OK && p) {
            memset(p, 0x42, 8);
            scl_alloc_slab_free(&slab, p);
            PASS();
        } else { FAIL("alloc 8 failed"); }
        scl_alloc_slab_destroy(&slab);
    }

    TEST("alloc various sizes");
    {
        scl_alloc_slab_t slab;
        scl_alloc_slab_init(&slab);
        void *p1, *p2, *p3;
        int ok = 1;
        ok = ok && (scl_alloc_slab_alloc(&slab, 8, &p1) == SCL_OK);
        ok = ok && (scl_alloc_slab_alloc(&slab, 64, &p2) == SCL_OK);
        ok = ok && (scl_alloc_slab_alloc(&slab, 1024, &p3) == SCL_OK);
        if (ok) {
            memset(p1, 0x11, 8);
            memset(p2, 0x22, 64);
            memset(p3, 0x33, 1024);
            scl_alloc_slab_free(&slab, p1);
            scl_alloc_slab_free(&slab, p2);
            scl_alloc_slab_free(&slab, p3);
            PASS();
        } else { FAIL("multi alloc failed"); }
        scl_alloc_slab_destroy(&slab);
    }

    TEST("stress 5000 allocs/frees across classes");
    {
        scl_alloc_slab_t slab;
        scl_alloc_slab_init(&slab);
        void *ptrs[5000];
        int ok = 1;
        for (int i = 0; i < 5000; i++) {
            size_t sz = (size_t)(1 << ((i % 9) + 3)); // 8 to 2048
            if (scl_alloc_slab_alloc(&slab, sz, &ptrs[i]) != SCL_OK) {
                ok = 0; break;
            }
        }
        for (int i = 0; i < 5000; i++)
            scl_alloc_slab_free(&slab, ptrs[i]);
        if (ok) { PASS(); } else { FAIL("stress failed"); }
        scl_alloc_slab_destroy(&slab);
    }

    TEST("NULL slab returns ERR_NULL_PTR");
    {
        void *p;
        if (scl_alloc_slab_alloc(NULL, 32, &p) == SCL_ERR_NULL_PTR) { PASS(); }
        else { FAIL("expected NULL_PTR"); }
    }

    TEST("free from wrong slab returns ERR_INVALID_ARG");
    {
        scl_alloc_slab_t slab;
        scl_alloc_slab_init(&slab);
        int dummy;
        if (scl_alloc_slab_free(&slab, &dummy) == SCL_ERR_INVALID_ARG) { PASS(); }
        else { FAIL("expected INVALID_ARG"); }
        scl_alloc_slab_destroy(&slab);
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
