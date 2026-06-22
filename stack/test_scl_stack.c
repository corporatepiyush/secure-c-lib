#include "scl_stack.h"
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
    TEST("basic LIFO");
    scl_stack_t s;
    scl_stack_init(&s, sizeof(int), 0);
    assert(scl_stack_empty(&s));
    for (int i = 0; i < 100; i++) assert(scl_stack_push(&s, &i) == SCL_OK);
    assert(scl_stack_count(&s) == 100);
    for (int i = 99; i >= 0; i--) {
        int v; scl_stack_pop(&s, &v); assert(v == i);
    }
    assert(scl_stack_empty(&s));
    assert(scl_stack_pop(&s, &(int){0}) == SCL_ERR_EMPTY);
    scl_stack_destroy(&s);
    PASS();
}

static void test_search(void)
{
    TEST("search");
    scl_stack_t s;
    scl_stack_init(&s, sizeof(int), 0);
    for (int i = 0; i < 10; i++) scl_stack_push(&s, &i);
    size_t idx;
    int key = 5;
    assert(scl_stack_search(&s, &key, cmp_int, &idx) == SCL_OK && idx == 5);
    key = 999;
    assert(scl_stack_search(&s, &key, cmp_int, &idx) == SCL_ERR_NOT_FOUND);
    scl_stack_destroy(&s);
    PASS();
}

int main(void)
{
    printf("=== scl_stack tests ===\n");
    test_basic();
    test_search();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
