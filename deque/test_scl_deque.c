#include "scl_deque.h"
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

static void test_push_pop(void)
{
    TEST("push/pop both ends");
    scl_deque_t d;
    scl_deque_init(&d, sizeof(int), 0);
    for (int i = 1; i <= 5; i++) {
        scl_deque_push_back(&d, &i);
        scl_deque_push_front(&d, &i);
    }
    assert(scl_deque_count(&d) == 10);
    for (int i = 5; i >= 1; i--) {
        int v; scl_deque_pop_front(&d, &v); assert(v == i);
        scl_deque_pop_back(&d, &v); assert(v == i);
    }
    scl_deque_destroy(&d);
    PASS();
}

static void test_search(void)
{
    TEST("search");
    scl_deque_t d;
    scl_deque_init(&d, sizeof(int), 0);
    for (int i = 0; i < 10; i++) scl_deque_push_back(&d, &i);
    size_t idx;
    int key = 5;
    assert(scl_deque_search(&d, &key, cmp_int, &idx) == SCL_OK && idx == 5);
    key = 999;
    assert(scl_deque_search(&d, &key, cmp_int, &idx) == SCL_ERR_NOT_FOUND);
    scl_deque_destroy(&d);
    PASS();
}

int main(void)
{
    printf("=== scl_deque tests ===\n");
    test_push_pop();
    test_search();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
