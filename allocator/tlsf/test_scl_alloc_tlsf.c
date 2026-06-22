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

    TEST("init and destroy");
    {
        scl_alloc_tlsf_t tlsf;
        scl_error_t e = scl_alloc_tlsf_init(&tlsf, 65536);
        if (e == SCL_OK) { PASS(); } else { FAIL("init failed"); }
        scl_alloc_tlsf_destroy(&tlsf);
    }

    TEST("alloc and free");
    {
        scl_alloc_tlsf_t tlsf;
        scl_alloc_tlsf_init(&tlsf, 65536);
        void *p = NULL;
        scl_error_t e = scl_alloc_tlsf_alloc(&tlsf, 128, &p);
        if (e == SCL_OK && p) {
            memset(p, 0xBB, 128);
            e = scl_alloc_tlsf_free(&tlsf, p);
            if (e == SCL_OK) { PASS(); } else { FAIL("free failed"); }
        } else { FAIL("alloc failed"); }
        scl_alloc_tlsf_destroy(&tlsf);
    }

    TEST("multiple sizes");
    {
        scl_alloc_tlsf_t tlsf;
        scl_alloc_tlsf_init(&tlsf, 65536);
        void *ptrs[10];
        int ok = 1;
        size_t sizes[] = {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
        for (int i = 0; i < 10; i++) {
            if (scl_alloc_tlsf_alloc(&tlsf, sizes[i], &ptrs[i]) != SCL_OK) {
                ok = 0; break;
            }
            memset(ptrs[i], (unsigned char)i, sizes[i]);
        }
        for (int i = 0; i < 10; i++)
            scl_alloc_tlsf_free(&tlsf, ptrs[i]);
        if (ok) { PASS(); } else { FAIL("multi size failed"); }
        scl_alloc_tlsf_destroy(&tlsf);
    }

    TEST("NULL checks");
    {
        void *p;
        if (scl_alloc_tlsf_alloc(NULL, 64, &p) == SCL_ERR_NULL_PTR) { PASS(); }
        else { FAIL("expected NULL_PTR"); }
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
