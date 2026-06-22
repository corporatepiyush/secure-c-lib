#include "concurrent_deque.h"
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
    scl_atomic_deque_t dq;
    assert(scl_atomic_deque_init(scl_allocator_default(), &dq, sizeof(int), 100) == SCL_OK);
    assert(scl_atomic_deque_empty(&dq));
    scl_atomic_deque_destroy(scl_allocator_default(), &dq);
    PASS();
}

static void test_push_pop(void)
{
    TEST("push/pop front/back");
    scl_atomic_deque_t dq;
    scl_atomic_deque_init(scl_allocator_default(), &dq, sizeof(int), 100);
    for (int i = 0; i < 50; i++) {
        scl_atomic_deque_push_back(scl_allocator_default(), &dq, &i);
        scl_atomic_deque_push_front(scl_allocator_default(), &dq, &i);
    }
    assert(scl_atomic_deque_count(&dq) == 100);
    int v;
    for (int i = 49; i >= 0; i--) {
        scl_atomic_deque_pop_front( &dq, &v); assert(v == i);
        scl_atomic_deque_pop_back( &dq, &v); assert(v == i);
    }
    assert(scl_atomic_deque_empty(&dq));
    scl_atomic_deque_destroy(scl_allocator_default(), &dq);
    PASS();
}

static void test_empty_full(void)
{
    TEST("empty and full checks");
    scl_atomic_deque_t dq;
    scl_atomic_deque_init(scl_allocator_default(), &dq, sizeof(int), 3);
    assert(scl_atomic_deque_pop_front( &dq, &(int){0}) == SCL_ERR_EMPTY);
    assert(scl_atomic_deque_pop_back( &dq, &(int){0}) == SCL_ERR_EMPTY);
    int v = 1; scl_atomic_deque_push_back(scl_allocator_default(), &dq, &v);
    v = 2; scl_atomic_deque_push_back(scl_allocator_default(), &dq, &v);
    v = 3; scl_atomic_deque_push_back(scl_allocator_default(), &dq, &v);
    v = 4; assert(scl_atomic_deque_push_back(scl_allocator_default(), &dq, &v) == SCL_ERR_FULL);
    scl_atomic_deque_destroy(scl_allocator_default(), &dq);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_atomic_deque_init(scl_allocator_default(), NULL, sizeof(int), 10) == SCL_ERR_NULL_PTR);
    scl_atomic_deque_destroy(scl_allocator_default(), NULL);
    PASS();
}

static void *push_back_thread(void *arg)
{
    scl_atomic_deque_t *dq = (scl_atomic_deque_t *)arg;
    for (int i = 0; i < 30; i++) scl_atomic_deque_push_back(scl_allocator_default(), dq, &i);
    return NULL;
}

static void test_concurrent_push(void)
{
    TEST("concurrent push back 2 threads");
    scl_atomic_deque_t dq;
    scl_atomic_deque_init(scl_allocator_default(), &dq, sizeof(int), 100);
    pthread_t t1, t2;
    pthread_create(&t1, NULL, push_back_thread, &dq);
    pthread_create(&t2, NULL, push_back_thread, &dq);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    assert(scl_atomic_deque_count(&dq) == 60);
    scl_atomic_deque_destroy(scl_allocator_default(), &dq);
    PASS();
}

int main(void)
{
    printf("=== scl_deque tests ===\n");
    test_init_destroy();
    test_push_pop();
    test_empty_full();
    test_null();
    test_concurrent_push();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
