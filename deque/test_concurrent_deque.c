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
    scl_concurrent_deque_t dq;
    assert(scl_concurrent_deque_init(&dq, sizeof(int), 100) == SCL_OK);
    assert(scl_concurrent_deque_empty(&dq));
    scl_concurrent_deque_destroy(&dq);
    PASS();
}

static void test_push_pop(void)
{
    TEST("push/pop front/back");
    scl_concurrent_deque_t dq;
    scl_concurrent_deque_init(&dq, sizeof(int), 100);
    for (int i = 0; i < 50; i++) {
        scl_concurrent_deque_push_back(&dq, &i);
        scl_concurrent_deque_push_front(&dq, &i);
    }
    assert(scl_concurrent_deque_count(&dq) == 100);
    int v;
    for (int i = 49; i >= 0; i--) {
        scl_concurrent_deque_pop_front(&dq, &v); assert(v == i);
        scl_concurrent_deque_pop_back(&dq, &v); assert(v == i);
    }
    assert(scl_concurrent_deque_empty(&dq));
    scl_concurrent_deque_destroy(&dq);
    PASS();
}

static void test_empty_full(void)
{
    TEST("empty and full checks");
    scl_concurrent_deque_t dq;
    scl_concurrent_deque_init(&dq, sizeof(int), 3);
    assert(scl_concurrent_deque_pop_front(&dq, &(int){0}) == SCL_ERR_EMPTY);
    assert(scl_concurrent_deque_pop_back(&dq, &(int){0}) == SCL_ERR_EMPTY);
    int v = 1; scl_concurrent_deque_push_back(&dq, &v);
    v = 2; scl_concurrent_deque_push_back(&dq, &v);
    v = 3; scl_concurrent_deque_push_back(&dq, &v);
    v = 4; assert(scl_concurrent_deque_push_back(&dq, &v) == SCL_ERR_FULL);
    scl_concurrent_deque_destroy(&dq);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_concurrent_deque_init(NULL, sizeof(int), 10) == SCL_ERR_NULL_PTR);
    scl_concurrent_deque_destroy(NULL);
    PASS();
}

static void *push_back_thread(void *arg)
{
    scl_concurrent_deque_t *dq = (scl_concurrent_deque_t *)arg;
    for (int i = 0; i < 30; i++) scl_concurrent_deque_push_back(dq, &i);
    return NULL;
}

static void test_concurrent_push(void)
{
    TEST("concurrent push back 2 threads");
    scl_concurrent_deque_t dq;
    scl_concurrent_deque_init(&dq, sizeof(int), 100);
    pthread_t t1, t2;
    pthread_create(&t1, NULL, push_back_thread, &dq);
    pthread_create(&t2, NULL, push_back_thread, &dq);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    assert(scl_concurrent_deque_count(&dq) == 60);
    scl_concurrent_deque_destroy(&dq);
    PASS();
}

int main(void)
{
    printf("=== scl_concurrent_deque tests ===\n");
    test_init_destroy();
    test_push_pop();
    test_empty_full();
    test_null();
    test_concurrent_push();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
