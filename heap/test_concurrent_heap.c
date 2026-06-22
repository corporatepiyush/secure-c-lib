#include "concurrent_heap.h"
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

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static void test_init_destroy(void)
{
    TEST("init and destroy");
    scl_atomic_heap_t heap;
    assert(scl_atomic_heap_init(scl_allocator_default(), &heap, sizeof(int), 100, cmp_int) == SCL_OK);
    assert(scl_atomic_heap_empty(&heap));
    scl_atomic_heap_destroy(scl_allocator_default(), &heap);
    PASS();
}

static void test_push_pop_peek(void)
{
    TEST("push, pop, peek (min-heap)");
    scl_atomic_heap_t heap;
    scl_atomic_heap_init(scl_allocator_default(), &heap, sizeof(int), 100, cmp_int);
    int vals[] = {50, 30, 70, 20, 40, 60, 10};
    for (int i = 0; i < 7; i++) assert(scl_atomic_heap_push(scl_allocator_default(), &heap, &vals[i]) == SCL_OK);
    assert(scl_atomic_heap_count(&heap) == 7);
    int v;
    assert(scl_atomic_heap_peek(&heap, &v) == SCL_OK && v == 10);
    int expected[] = {10, 20, 30, 40, 50, 60, 70};
    for (int i = 0; i < 7; i++) {
        assert(scl_atomic_heap_pop(&heap, &v) == SCL_OK);
        assert(v == expected[i]);
    }
    assert(scl_atomic_heap_empty(&heap));
    scl_atomic_heap_destroy(scl_allocator_default(), &heap);
    PASS();
}

static void test_empty_full(void)
{
    TEST("empty and full checks");
    scl_atomic_heap_t heap;
    scl_atomic_heap_init(scl_allocator_default(), &heap, sizeof(int), 3, cmp_int);
    assert(scl_atomic_heap_pop(&heap, &(int){0}) == SCL_ERR_EMPTY);
    assert(scl_atomic_heap_peek(&heap, &(int){0}) == SCL_ERR_EMPTY);
    int v;
    for (v = 0; v < 3; v++) assert(scl_atomic_heap_push(scl_allocator_default(), &heap, &v) == SCL_OK);
    assert(scl_atomic_heap_push(scl_allocator_default(), &heap, &v) == SCL_ERR_FULL);
    scl_atomic_heap_destroy(scl_allocator_default(), &heap);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_atomic_heap_init(scl_allocator_default(), NULL, sizeof(int), 10, cmp_int) == SCL_ERR_NULL_PTR);
    scl_atomic_heap_destroy(scl_allocator_default(), NULL);
    PASS();
}

static void *push_thread(void *arg)
{
    scl_atomic_heap_t *heap = (scl_atomic_heap_t *)arg;
    for (int i = 0; i < 30; i++) scl_atomic_heap_push(scl_allocator_default(), heap, &i);
    return NULL;
}

static void test_concurrent_push(void)
{
    TEST("concurrent push 2 threads");
    scl_atomic_heap_t heap;
    scl_atomic_heap_init(scl_allocator_default(), &heap, sizeof(int), 100, cmp_int);
    pthread_t t1, t2;
    pthread_create(&t1, NULL, push_thread, &heap);
    pthread_create(&t2, NULL, push_thread, &heap);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    assert(scl_atomic_heap_count(&heap) == 60);
    scl_atomic_heap_destroy(scl_allocator_default(), &heap);
    PASS();
}

int main(void)
{
    printf("=== scl_heap tests ===\n");
    test_init_destroy();
    test_push_pop_peek();
    test_empty_full();
    test_null();
    test_concurrent_push();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
