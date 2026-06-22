#include "scl_ringbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static void test_no_overwrite(void)
{
    TEST("push and pop (no overwrite)");
    scl_ringbuf_t rb;
    scl_ringbuf_init(&rb, sizeof(int), 3, false);
    int v = 1; scl_ringbuf_push(&rb, &v);
    v = 2; scl_ringbuf_push(&rb, &v);
    v = 3; scl_ringbuf_push(&rb, &v);
    assert(scl_ringbuf_full(&rb));
    assert(scl_ringbuf_push(&rb, &v) == SCL_ERR_FULL);
    scl_ringbuf_pop(&rb, &v); assert(v == 1);
    assert(!scl_ringbuf_full(&rb));
    scl_ringbuf_destroy(&rb);
    PASS();
}

static void test_overwrite(void)
{
    TEST("overwrite mode");
    scl_ringbuf_t rb;
    scl_ringbuf_init(&rb, sizeof(int), 2, true);
    int v1 = 1, v2 = 2, v3 = 3;
    scl_ringbuf_push(&rb, &v1);
    scl_ringbuf_push(&rb, &v2);
    scl_ringbuf_push(&rb, &v3);
    scl_ringbuf_pop(&rb, &v1); assert(v1 == 2);
    scl_ringbuf_pop(&rb, &v2); assert(v2 == 3);
    scl_ringbuf_destroy(&rb);
    PASS();
}

static void test_search(void)
{
    TEST("search");
    scl_ringbuf_t rb;
    scl_ringbuf_init(&rb, sizeof(int), 10, false);
    for (int i = 0; i < 10; i++) scl_ringbuf_push(&rb, &i);
    size_t idx;
    int key = 5;
    assert(scl_ringbuf_search(&rb, &key, cmp_int, &idx) == SCL_OK && idx == 5);
    key = 999;
    assert(scl_ringbuf_search(&rb, &key, cmp_int, &idx) == SCL_ERR_NOT_FOUND);
    scl_ringbuf_destroy(&rb);
    PASS();
}

int main(void)
{
    printf("=== scl_ringbuf tests ===\n");
    test_no_overwrite();
    test_overwrite();
    test_search();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
