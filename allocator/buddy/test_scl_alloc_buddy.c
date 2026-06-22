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

    TEST("create and destroy");
    {
        scl_allocator_t *buddy = scl_alloc_buddy_create(scl_allocator_default(), 1UL << 20);
        if (buddy) { PASS(); } else { FAIL("create failed"); }
        scl_alloc_buddy_destroy(buddy);
    }

    TEST("alloc and free");
    {
        scl_allocator_t *buddy = scl_alloc_buddy_create(scl_allocator_default(), 1UL << 20);
        void *p = scl_alloc(buddy, 128, 16);
        if (p) {
            memset(p, 0xCC, 128);
            scl_free(buddy, p);
            PASS();
        } else { FAIL("alloc failed"); }
        scl_alloc_buddy_destroy(buddy);
    }

    TEST("stress 500 allocs");
    {
        scl_allocator_t *buddy = scl_alloc_buddy_create(scl_allocator_default(), 1UL << 22);
        void *ptrs[500];
        int ok = 1;
        for (int i = 0; i < 500; i++) {
            size_t sz = (size_t)(1 << ((i % 10) + 4));
            ptrs[i] = scl_alloc(buddy, sz, 16);
            if (!ptrs[i]) { ok = 0; break; }
        }
        for (int i = 0; i < 500; i++)
            scl_free(buddy, ptrs[i]);
        if (ok) { PASS(); } else { FAIL("stress failed"); }
        scl_alloc_buddy_destroy(buddy);
    }

    TEST("NULL backing returns NULL");
    {
        scl_allocator_t *buddy = scl_alloc_buddy_create(NULL, 1UL << 20);
        if (!buddy) { PASS(); } else { FAIL("expected NULL"); }
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
