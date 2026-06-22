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

    TEST("create and destroy");
    {
        scl_allocator_t *slab = scl_alloc_slab_create(scl_allocator_default(), NULL, 0);
        if (slab) { PASS(); } else { FAIL("create failed"); }
        scl_alloc_slab_destroy(slab);
    }

    TEST("alloc small (16 bytes)");
    {
        scl_allocator_t *slab = scl_alloc_slab_create(scl_allocator_default(), NULL, 0);
        void *p = scl_alloc(slab, 16, 16);
        if (p) {
            memset(p, 0x42, 16);
            scl_free(slab, p);
            PASS();
        } else { FAIL("alloc 16 failed"); }
        scl_alloc_slab_destroy(slab);
    }

    TEST("alloc various sizes");
    {
        scl_allocator_t *slab = scl_alloc_slab_create(scl_allocator_default(), NULL, 0);
        void *p1 = scl_alloc(slab, 16, 16);
        void *p2 = scl_alloc(slab, 64, 16);
        void *p3 = scl_alloc(slab, 1024, 16);
        if (p1 && p2 && p3) {
            memset(p1, 0x11, 16);
            memset(p2, 0x22, 64);
            memset(p3, 0x33, 1024);
            scl_free(slab, p1);
            scl_free(slab, p2);
            scl_free(slab, p3);
            PASS();
        } else { FAIL("multi alloc failed"); }
        scl_alloc_slab_destroy(slab);
    }

    TEST("stress 5000 allocs/frees across classes");
    {
        scl_allocator_t *slab = scl_alloc_slab_create(scl_allocator_default(), NULL, 0);
        void *ptrs[5000];
        int ok = 1;
        for (int i = 0; i < 5000; i++) {
            size_t sz = (size_t)(1 << ((i % 9) + 4));
            ptrs[i] = scl_alloc(slab, sz, 16);
            if (!ptrs[i]) { ok = 0; break; }
        }
        for (int i = 0; i < 5000; i++)
            scl_free(slab, ptrs[i]);
        if (ok) { PASS(); } else { FAIL("stress failed"); }
        scl_alloc_slab_destroy(slab);
    }

    TEST("NULL slab returns NULL on alloc");
    {
        void *p = scl_alloc(NULL, 32, 16);
        if (!p) { PASS(); } else { FAIL("expected NULL"); }
    }

    TEST("custom bucket sizes");
    {
        size_t buckets[] = {32, 64, 128};
        scl_allocator_t *slab = scl_alloc_slab_create(scl_allocator_default(), buckets, 3);
        void *p = scl_alloc(slab, 32, 16);
        void *q = scl_alloc(slab, 64, 16);
        if (p && q) { PASS(); } else { FAIL("custom buckets failed"); }
        scl_free(slab, p);
        scl_free(slab, q);
        scl_alloc_slab_destroy(slab);
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
