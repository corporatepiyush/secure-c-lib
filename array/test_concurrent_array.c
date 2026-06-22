#include "concurrent_array.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static void test_init_destroy(void)
{
    TEST("init and destroy");
    scl_atomic_array_t arr;
    assert(scl_atomic_array_init(scl_allocator_default(), &arr, sizeof(int), 100) == SCL_OK);
    assert(scl_atomic_array_empty(&arr));
    assert(scl_atomic_array_count(&arr) == 0);
    scl_atomic_array_destroy(scl_allocator_default(), &arr);
    PASS();
}

static void test_push_pop(void)
{
    TEST("push_back and pop_back");
    scl_atomic_array_t arr;
    scl_atomic_array_init(scl_allocator_default(), &arr, sizeof(int), 100);
    for (int i = 0; i < 100; i++)
        assert(scl_atomic_array_push(scl_allocator_default(), &arr, &i) == SCL_OK);
    assert(scl_atomic_array_count(&arr) == 100);
    for (int i = 99; i >= 0; i--) {
        int val;
        assert(scl_atomic_array_pop( &arr, &val) == SCL_OK);
        assert(val == i);
    }
    assert(scl_atomic_array_empty(&arr));
    scl_atomic_array_destroy(scl_allocator_default(), &arr);
    PASS();
}

static void test_get_set(void)
{
    TEST("get and set");
    scl_atomic_array_t arr;
    scl_atomic_array_init(scl_allocator_default(), &arr, sizeof(int), 10);
    int x = 42; scl_atomic_array_push(scl_allocator_default(), &arr, &x);
    int val; assert(scl_atomic_array_get(&arr, 0, &val) == SCL_OK && val == 42);
    x = 99; assert(scl_atomic_array_set(&arr, 0, &x) == SCL_OK);
    assert(scl_atomic_array_get(&arr, 0, &val) == SCL_OK && val == 99);
    assert(scl_atomic_array_get(&arr, 1, &val) == SCL_ERR_INVALID_INDEX);
    scl_atomic_array_destroy(scl_allocator_default(), &arr);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_atomic_array_init(scl_allocator_default(), NULL, sizeof(int), 10) == SCL_ERR_NULL_PTR);
    assert(scl_atomic_array_push(scl_allocator_default(), NULL, &(int){0}) == SCL_ERR_NULL_PTR);
    assert(scl_atomic_array_pop( NULL, &(int){0}) == SCL_ERR_NULL_PTR);
    assert(scl_atomic_array_get(NULL, 0, &(int){0}) == SCL_ERR_NULL_PTR);
    scl_atomic_array_destroy(scl_allocator_default(), NULL);
    PASS();
}

static void test_empty_ops(void)
{
    TEST("empty ops");
    scl_atomic_array_t arr;
    scl_atomic_array_init(scl_allocator_default(), &arr, sizeof(int), 10);
    assert(scl_atomic_array_pop( &arr, &(int){0}) == SCL_ERR_EMPTY);
    assert(scl_atomic_array_get(&arr, 0, &(int){0}) == SCL_ERR_INVALID_INDEX);
    scl_atomic_array_destroy(scl_allocator_default(), &arr);
    PASS();
}

static void test_full(void)
{
    TEST("full array rejection");
    scl_atomic_array_t arr;
    scl_atomic_array_init(scl_allocator_default(), &arr, sizeof(int), 3);
    int v;
    for (v = 0; v < 3; v++) assert(scl_atomic_array_push(scl_allocator_default(), &arr, &v) == SCL_OK);
    assert(scl_atomic_array_push(scl_allocator_default(), &arr, &v) == SCL_ERR_FULL);
    scl_atomic_array_destroy(scl_allocator_default(), &arr);
    PASS();
}

typedef struct {
    scl_atomic_array_t *arr;
    int start;
    int count;
} thread_arg_t;

static void *push_thread(void *arg)
{
    thread_arg_t *ta = (thread_arg_t *)arg;
    for (int i = 0; i < ta->count; i++) {
        int v = ta->start + i;
        scl_atomic_array_push(scl_allocator_default(), ta->arr, &v);
    }
    return NULL;
}

static void test_concurrent_push(void)
{
    TEST("concurrent push 2 threads x 50");
    scl_atomic_array_t arr;
    scl_atomic_array_init(scl_allocator_default(), &arr, sizeof(int), 200);
    pthread_t t1, t2;
    thread_arg_t a1 = {&arr, 0, 50};
    thread_arg_t a2 = {&arr, 50, 50};
    pthread_create(&t1, NULL, push_thread, &a1);
    pthread_create(&t2, NULL, push_thread, &a2);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    assert(scl_atomic_array_count(&arr) == 100);
    int found[100] = {0};
    for (size_t i = 0; i < 100; i++) {
        int v;
        scl_atomic_array_get(&arr, i, &v);
        assert(v >= 0 && v < 100);
        found[v] = 1;
    }
    for (int i = 0; i < 100; i++) assert(found[i]);
    scl_atomic_array_destroy(scl_allocator_default(), &arr);
    PASS();
}

int main(void)
{
    printf("=== scl_array tests ===\n");
    test_init_destroy();
    test_push_pop();
    test_get_set();
    test_null();
    test_empty_ops();
    test_full();
    test_concurrent_push();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
