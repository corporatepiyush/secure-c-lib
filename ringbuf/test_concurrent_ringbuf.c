#include "concurrent_ringbuf.h"
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
    scl_concurrent_ringbuf_t rb;
    assert(scl_concurrent_ringbuf_init(&rb, sizeof(int), 100) == SCL_OK);
    assert(scl_concurrent_ringbuf_empty(&rb));
    scl_concurrent_ringbuf_destroy(&rb);
    PASS();
}

static void test_push_pop_peek(void)
{
    TEST("push, pop, peek");
    scl_concurrent_ringbuf_t rb;
    scl_concurrent_ringbuf_init(&rb, sizeof(int), 100);
    for (int i = 0; i < 100; i++)
        assert(scl_concurrent_ringbuf_push(&rb, &i) == SCL_OK);
    assert(scl_concurrent_ringbuf_count(&rb) == 100);
    for (int i = 0; i < 100; i++) {
        int v;
        assert(scl_concurrent_ringbuf_peek(&rb, 0, &v) == SCL_OK && v == i);
        assert(scl_concurrent_ringbuf_pop(&rb, &v) == SCL_OK && v == i);
    }
    assert(scl_concurrent_ringbuf_empty(&rb));
    scl_concurrent_ringbuf_destroy(&rb);
    PASS();
}

static void test_full_empty(void)
{
    TEST("full and empty edge cases");
    scl_concurrent_ringbuf_t rb;
    scl_concurrent_ringbuf_init(&rb, sizeof(int), 3);
    assert(scl_concurrent_ringbuf_pop(&rb, &(int){0}) == SCL_ERR_EMPTY);
    for (int i = 0; i < 3; i++) assert(scl_concurrent_ringbuf_push(&rb, &i) == SCL_OK);
    assert(scl_concurrent_ringbuf_push(&rb, &(int){3}) == SCL_ERR_FULL);
    scl_concurrent_ringbuf_destroy(&rb);
    PASS();
}

static void test_null(void)
{
    TEST("null checks");
    assert(scl_concurrent_ringbuf_init(NULL, sizeof(int), 10) == SCL_ERR_NULL_PTR);
    scl_concurrent_ringbuf_destroy(NULL);
    PASS();
}

static void *producer_thread(void *arg)
{
    scl_concurrent_ringbuf_t *rb = (scl_concurrent_ringbuf_t *)arg;
    for (int i = 0; i < 50; i++) scl_concurrent_ringbuf_push(rb, &i);
    return NULL;
}

static void *consumer_thread(void *arg)
{
    scl_concurrent_ringbuf_t *rb = (scl_concurrent_ringbuf_t *)arg;
    int v;
    for (int i = 0; i < 50; i++) scl_concurrent_ringbuf_pop(rb, &v);
    return NULL;
}

static void test_spsc(void)
{
    TEST("SPSC producer/consumer");
    scl_concurrent_ringbuf_t rb;
    scl_concurrent_ringbuf_init(&rb, sizeof(int), 100);
    pthread_t prod, cons;
    pthread_create(&prod, NULL, producer_thread, &rb);
    pthread_create(&cons, NULL, consumer_thread, &rb);
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);
    assert(scl_concurrent_ringbuf_empty(&rb));
    scl_concurrent_ringbuf_destroy(&rb);
    PASS();
}

int main(void)
{
    printf("=== scl_concurrent_ringbuf tests ===\n");
    test_init_destroy();
    test_push_pop_peek();
    test_full_empty();
    test_null();
    test_spsc();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
