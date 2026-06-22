#include "scl_alloc_buddy.h"
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
    printf("=== scl_alloc_buddy tests ===\n");

    TEST("init and destroy");
    {
        scl_alloc_buddy_t buddy;
        scl_error_t e = scl_alloc_buddy_init(&buddy, 16);
        if (e == SCL_OK) { PASS(); } else { FAIL("init failed"); }
        scl_alloc_buddy_destroy(&buddy);
    }

    TEST("alloc and free");
    {
        scl_alloc_buddy_t buddy;
        scl_alloc_buddy_init(&buddy, 16);
        void *p = NULL;
        scl_error_t e = scl_alloc_buddy_alloc(&buddy, 128, &p);
        if (e == SCL_OK && p) {
            memset(p, 0xCC, 128);
            e = scl_alloc_buddy_free(&buddy, p);
            if (e == SCL_OK) { PASS(); } else { FAIL("free failed"); }
        } else { FAIL("alloc failed"); }
        scl_alloc_buddy_destroy(&buddy);
    }

    TEST("stress 500 allocs");
    {
        scl_alloc_buddy_t buddy;
        scl_alloc_buddy_init(&buddy, 18);
        void *ptrs[500];
        int ok = 1;
        for (int i = 0; i < 500; i++) {
            size_t sz = (size_t)(1 << ((i % 10) + 4));
            if (scl_alloc_buddy_alloc(&buddy, sz, &ptrs[i]) != SCL_OK) {
                ok = 0; break;
            }
        }
        for (int i = 0; i < 500; i++)
            scl_alloc_buddy_free(&buddy, ptrs[i]);
        if (ok) { PASS(); } else { FAIL("stress failed"); }
        scl_alloc_buddy_destroy(&buddy);
    }

    TEST("NULL checks");
    {
        void *p;
        if (scl_alloc_buddy_alloc(NULL, 64, &p) == SCL_ERR_NULL_PTR) { PASS(); }
        else { FAIL("expected NULL_PTR"); }
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
