#include "scl_queue.h"
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

static void test_basic(void)
{
    TEST("basic FIFO");
    scl_queue_t q;
    scl_queue_init(&q, sizeof(int), 0);
    for (int i = 0; i < 100; i++) assert(scl_queue_enqueue(&q, &i) == SCL_OK);
    assert(scl_queue_count(&q) == 100);
    for (int i = 0; i < 100; i++) {
        int v; scl_queue_dequeue(&q, &v); assert(v == i);
    }
    assert(scl_queue_empty(&q));
    scl_queue_destroy(&q);
    PASS();
}

static void test_search(void)
{
    TEST("search");
    scl_queue_t q;
    scl_queue_init(&q, sizeof(int), 0);
    for (int i = 0; i < 10; i++) scl_queue_enqueue(&q, &i);
    size_t idx;
    int key = 5;
    assert(scl_queue_search(&q, &key, cmp_int, &idx) == SCL_OK && idx == 5);
    key = 999;
    assert(scl_queue_search(&q, &key, cmp_int, &idx) == SCL_ERR_NOT_FOUND);
    scl_queue_destroy(&q);
    PASS();
}

int main(void)
{
    printf("=== scl_queue tests ===\n");
    test_basic();
    test_search();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
