#include "scl_alloc_tlsf.h"
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
    printf("=== scl_alloc_tlsf tests ===\n");

    TEST("create and destroy");
    {
        scl_allocator_t *tlsf = scl_alloc_tlsf_create(scl_allocator_default(), 65536);
        if (tlsf) { PASS(); } else { FAIL("create failed"); }
        scl_alloc_tlsf_destroy(tlsf);
    }

    TEST("alloc and free");
    {
        scl_allocator_t *tlsf = scl_alloc_tlsf_create(scl_allocator_default(), 65536);
        void *p = scl_alloc(tlsf, 128, 16);
        if (p) {
            memset(p, 0xBB, 128);
            scl_free(tlsf, p);
            PASS();
        } else { FAIL("alloc failed"); }
        scl_alloc_tlsf_destroy(tlsf);
    }

    TEST("multiple sizes");
    {
        scl_allocator_t *tlsf = scl_alloc_tlsf_create(scl_allocator_default(), 65536);
        void *ptrs[10];
        int ok = 1;
        size_t sizes[] = {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
        for (int i = 0; i < 10; i++) {
            ptrs[i] = scl_alloc(tlsf, sizes[i], 16);
            if (!ptrs[i]) { ok = 0; break; }
            memset(ptrs[i], (unsigned char)i, sizes[i]);
        }
        for (int i = 0; i < 10; i++)
            scl_free(tlsf, ptrs[i]);
        if (ok) { PASS(); } else { FAIL("multi size failed"); }
        scl_alloc_tlsf_destroy(tlsf);
    }

    TEST("NULL backing returns NULL");
    {
        scl_allocator_t *tlsf = scl_alloc_tlsf_create(NULL, 65536);
        if (!tlsf) { PASS(); } else { FAIL("expected NULL"); }
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
