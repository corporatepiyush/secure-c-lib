#include "concurrent_queue.h"
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
    scl_atomic_queue_t q;
    assert(scl_atomic_queue_init(scl_allocator_default(), &q, sizeof(int)) == SCL_OK);
    assert(scl_atomic_queue_empty(&q));
    scl_atomic_queue_destroy(scl_allocator_default(), &q);
    PASS();
}

static void test_enqueue_dequeue_fifo(void)
{
    TEST("enqueue/dequeue FIFO ordering");
    scl_atomic_queue_t q;
    scl_atomic_queue_init(scl_allocator_default(), &q, sizeof(int));
    for (int i = 0; i < 100; i++)
        assert(scl_atomic_queue_enqueue(scl_allocator_default(), &q, &i) == SCL_OK);
    assert(scl_atomic_queue_count(&q) == 100);
    for (int i = 0; i < 100; i++) {
        int v;
        assert(scl_atomic_queue_dequeue(scl_allocator_default(), &q, &v) == SCL_OK);
        assert(v == i);
    }
    assert(scl_atomic_queue_empty(&q));
    assert(scl_atomic_queue_dequeue(scl_allocator_default(), &q, &(int){0}) == SCL_ERR_EMPTY);
    scl_atomic_queue_destroy(scl_allocator_default(), &q);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_atomic_queue_init(scl_allocator_default(), NULL, sizeof(int)) == SCL_ERR_NULL_PTR);
    assert(scl_atomic_queue_enqueue(scl_allocator_default(), NULL, &(int){0}) == SCL_ERR_NULL_PTR);
    assert(scl_atomic_queue_dequeue(scl_allocator_default(), NULL, &(int){0}) == SCL_ERR_NULL_PTR);
    scl_atomic_queue_destroy(scl_allocator_default(), NULL);
    PASS();
}

typedef struct {
    scl_atomic_queue_t *q;
    int start;
    int count;
} thread_arg_t;

static void *enqueue_thread(void *arg)
{
    thread_arg_t *ta = (thread_arg_t *)arg;
    for (int i = 0; i < ta->count; i++) {
        int v = ta->start + i;
        scl_atomic_queue_enqueue(scl_allocator_default(), ta->q, &v);
    }
    return NULL;
}

static void test_concurrent_enqueue(void)
{
    TEST("concurrent enqueue 2 threads x 50");
    scl_atomic_queue_t q;
    scl_atomic_queue_init(scl_allocator_default(), &q, sizeof(int));
    pthread_t t1, t2;
    thread_arg_t a1 = {&q, 0, 50};
    thread_arg_t a2 = {&q, 50, 50};
    pthread_create(&t1, NULL, enqueue_thread, &a1);
    pthread_create(&t2, NULL, enqueue_thread, &a2);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    assert(scl_atomic_queue_count(&q) == 100);
    int found[100] = {0};
    for (int i = 0; i < 100; i++) {
        int v;
        scl_atomic_queue_dequeue(scl_allocator_default(), &q, &v);
        assert(v >= 0 && v < 100);
        found[v] = 1;
    }
    for (int i = 0; i < 100; i++) assert(found[i]);
    scl_atomic_queue_destroy(scl_allocator_default(), &q);
    PASS();
}

int main(void)
{
    printf("=== scl_queue tests ===\n");
    test_init_destroy();
    test_enqueue_dequeue_fifo();
    test_null();
    test_concurrent_enqueue();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
