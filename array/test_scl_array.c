#include "scl_array.h"
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

static void test_init_destroy(void)
{
    TEST("init and destroy");
    scl_array_t arr;
    assert(scl_array_init(&arr, sizeof(int), 0) == SCL_OK);
    assert(scl_array_empty(&arr));
    assert(scl_array_count(&arr) == 0);
    scl_array_destroy(&arr);
    PASS();
}

static void test_push_pop(void)
{
    TEST("push and pop");
    scl_array_t arr;
    scl_array_init(&arr, sizeof(int), 0);
    for (int i = 0; i < 100; i++)
        assert(scl_array_push(&arr, &i) == SCL_OK);
    assert(scl_array_count(&arr) == 100);
    for (int i = 99; i >= 0; i--) {
        int val;
        assert(scl_array_pop(&arr, &val) == SCL_OK);
        assert(val == i);
    }
    assert(scl_array_empty(&arr));
    scl_array_destroy(&arr);
    PASS();
}

static void test_get_set(void)
{
    TEST("get and set");
    scl_array_t arr;
    scl_array_init(&arr, sizeof(int), 10);
    int x = 42; scl_array_push(&arr, &x);
    int val; assert(scl_array_get(&arr, 0, &val) == SCL_OK && val == 42);
    x = 99; assert(scl_array_set(&arr, 0, &x) == SCL_OK);
    assert(scl_array_get(&arr, 0, &val) == SCL_OK && val == 99);
    assert(scl_array_get(&arr, 1, &val) == SCL_ERR_INVALID_INDEX);
    scl_array_destroy(&arr);
    PASS();
}

static void test_insert_remove(void)
{
    TEST("insert and remove");
    scl_array_t arr;
    scl_array_init(&arr, sizeof(int), 4);
    for (int i = 0; i < 3; i++) { int v = i * 10; scl_array_push(&arr, &v); }
    int v = 99; assert(scl_array_insert(&arr, 1, &v) == SCL_OK);
    assert(scl_array_get(&arr, 1, &v) == SCL_OK && v == 99);
    assert(scl_array_remove(&arr, 1, &v) == SCL_OK && v == 99);
    assert(scl_array_count(&arr) == 3);
    scl_array_destroy(&arr);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_array_init(NULL, sizeof(int), 0) == SCL_ERR_NULL_PTR);
    assert(scl_array_push(NULL, &(int){0}) == SCL_ERR_NULL_PTR);
    assert(scl_array_pop(NULL, &(int){0}) == SCL_ERR_NULL_PTR);
    assert(scl_array_get(NULL, 0, &(int){0}) == SCL_ERR_NULL_PTR);
    scl_array_destroy(NULL);
    PASS();
}

static void test_edge(void)
{
    TEST("empty ops");
    scl_array_t arr;
    scl_array_init(&arr, sizeof(int), 0);
    assert(scl_array_pop(&arr, &(int){0}) == SCL_ERR_EMPTY);
    assert(scl_array_get(&arr, 0, &(int){0}) == SCL_ERR_INVALID_INDEX);
    scl_array_destroy(&arr);
    PASS();

    TEST("zero element size rejection");
    assert(scl_array_init(&arr, 0, 10) == SCL_ERR_INVALID_ARG);
    PASS();

    TEST("large data integrity");
    scl_array_init(&arr, sizeof(int), 0);
    for (int i = 0; i < 1000; i++) scl_array_push(&arr, &i);
    for (int i = 0; i < 1000; i++) {
        int v; scl_array_get(&arr, i, &v); assert(v == i);
    }
    scl_array_destroy(&arr);
    PASS();

    TEST("type safety with structs");
    typedef struct { int x; double y; } point;
    scl_array_t pa;
    scl_array_init(&pa, sizeof(point), 0);
    for (int i = 0; i < 10; i++) {
        point p = {i, i * 1.5};
        scl_array_push(&pa, &p);
    }
    for (int i = 0; i < 10; i++) {
        point p; scl_array_get(&pa, i, &p);
        assert(p.x == i && p.y == i * 1.5);
    }
    scl_array_destroy(&pa);
    PASS();
}

static void test_search(void)
{
    TEST("linear search");
    scl_array_t arr;
    scl_array_init(&arr, sizeof(int), 0);
    for (int i = 0; i < 10; i++) { int v = i * 10; scl_array_push(&arr, &v); }
    size_t idx;
    int key = 50;
    assert(scl_array_linear_search(&arr, &key, cmp_int, &idx) == SCL_OK && idx == 5);
    key = 999;
    assert(scl_array_linear_search(&arr, &key, cmp_int, &idx) == SCL_ERR_NOT_FOUND);
    scl_array_destroy(&arr);
    PASS();

    TEST("binary search");
    scl_array_init(&arr, sizeof(int), 0);
    for (int i = 0; i < 10; i++) { int v = i * 10; scl_array_push(&arr, &v); }
    key = 50;
    assert(scl_array_binary_search(&arr, &key, cmp_int, &idx) == SCL_OK && idx == 5);
    key = 999;
    assert(scl_array_binary_search(&arr, &key, cmp_int, &idx) == SCL_ERR_NOT_FOUND);
    scl_array_destroy(&arr);
    PASS();
}

int main(void)
{
    printf("=== scl_array tests ===\n");
    test_init_destroy();
    test_push_pop();
    test_get_set();
    test_insert_remove();
    test_null();
    test_edge();
    test_search();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
