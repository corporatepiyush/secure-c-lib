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

    TEST("create and destroy");
    {
        scl_allocator_t *arena = scl_alloc_arena_create(scl_allocator_default(), 4096);
        if (arena) { PASS(); } else { FAIL("create failed"); }
        scl_alloc_arena_destroy(arena);
    }

    TEST("basic alloc");
    {
        scl_allocator_t *arena = scl_alloc_arena_create(scl_allocator_default(), 4096);
        void *p = scl_alloc(arena, 128, 16);
        if (p) {
            memset(p, 0xAA, 128);
            PASS();
        } else { FAIL("alloc failed"); }
        scl_alloc_arena_destroy(arena);
    }

    TEST("multiple allocs");
    {
        scl_allocator_t *arena = scl_alloc_arena_create(scl_allocator_default(), 4096);
        void *a = scl_alloc(arena, 64, 8);
        void *b = scl_alloc(arena, 128, 16);
        void *c = scl_alloc(arena, 256, 32);
        if (a && b && c) {
            memset(a, 0x11, 64);
            memset(b, 0x22, 128);
            memset(c, 0x33, 256);
            PASS();
        } else { FAIL("multialloc failed"); }
        scl_alloc_arena_destroy(arena);
    }

    TEST("reset reuses memory");
    {
        scl_allocator_t *arena = scl_alloc_arena_create(scl_allocator_default(), 4096);
        void *p1 = scl_alloc(arena, 2048, 8);
        scl_alloc_arena_reset(arena);
        void *p2 = scl_alloc(arena, 2048, 8);
        if (p1 == p2) { PASS(); } else { FAIL("reset should reuse"); }
        scl_alloc_arena_destroy(arena);
    }

    TEST("alignment");
    {
        scl_allocator_t *arena = scl_alloc_arena_create(scl_allocator_default(), 4096);
        void *p = scl_alloc(arena, 1, 256);
        if (((uintptr_t)p & 255) == 0) { PASS(); }
        else { FAIL("misaligned"); }
        scl_alloc_arena_destroy(arena);
    }

    TEST("NULL backing returns NULL");
    {
        scl_allocator_t *arena = scl_alloc_arena_create(NULL, 4096);
        if (!arena) { PASS(); } else { FAIL("expected NULL"); }
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
