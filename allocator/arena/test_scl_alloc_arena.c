#include "scl_alloc_arena.h"
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
    printf("=== scl_alloc_arena tests ===\n");

    TEST("init and destroy");
    {
        scl_alloc_arena_t arena;
        scl_error_t e = scl_alloc_arena_init(&arena, 4096);
        if (e == SCL_OK) { PASS(); } else { FAIL("init failed"); }
        scl_alloc_arena_destroy(&arena);
    }

    TEST("basic alloc");
    {
        scl_alloc_arena_t arena;
        scl_alloc_arena_init(&arena, 4096);
        void *p = NULL;
        scl_error_t e = scl_alloc_arena_alloc(&arena, 128, 16, &p);
        if (e == SCL_OK && p) {
            memset(p, 0xAA, 128);
            PASS();
        } else { FAIL("alloc failed"); }
        scl_alloc_arena_destroy(&arena);
    }

    TEST("multiple allocs");
    {
        scl_alloc_arena_t arena;
        scl_alloc_arena_init(&arena, 4096);
        void *a, *b, *c;
        int ok = scl_alloc_arena_alloc(&arena, 64, 8, &a) == SCL_OK;
        ok = ok && scl_alloc_arena_alloc(&arena, 128, 16, &b) == SCL_OK;
        ok = ok && scl_alloc_arena_alloc(&arena, 256, 32, &c) == SCL_OK;
        if (ok) {
            memset(a, 0x11, 64);
            memset(b, 0x22, 128);
            memset(c, 0x33, 256);
            PASS();
        } else { FAIL("multialloc failed"); }
        scl_alloc_arena_destroy(&arena);
    }

    TEST("reset reuses memory");
    {
        scl_alloc_arena_t arena;
        scl_alloc_arena_init(&arena, 4096);
        void *p1 = NULL;
        scl_alloc_arena_alloc(&arena, 2048, 8, &p1);
        scl_alloc_arena_reset(&arena);
        void *p2 = NULL;
        scl_alloc_arena_alloc(&arena, 2048, 8, &p2);
        if (p1 == p2) { PASS(); } else { FAIL("reset should reuse"); }
        scl_alloc_arena_destroy(&arena);
    }

    TEST("alignment");
    {
        scl_alloc_arena_t arena;
        scl_alloc_arena_init(&arena, 4096);
        void *p = NULL;
        scl_alloc_arena_alloc(&arena, 1, 256, &p);
        if (((uintptr_t)p & 255) == 0) { PASS(); }
        else { FAIL("misaligned"); }
        scl_alloc_arena_destroy(&arena);
    }

    TEST("NULL checks");
    {
        if (scl_alloc_arena_init(NULL, 4096) == SCL_ERR_NULL_PTR) { PASS(); }
        else { FAIL("expected NULL_PTR"); }
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
