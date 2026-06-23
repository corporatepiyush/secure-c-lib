#include "scl_test.h"
#include "scl_concurrent_ringbuf.h"
#include <pthread.h>
#include <stdatomic.h>

#define CAPACITY 256
#define N_ITEMS  512

static void test_cringbuf_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("CRingbuf: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_ringbuf_t rb;
    scl_error_t err = scl_cringbuf_init(alloc, &rb, sizeof(int), CAPACITY);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_cringbuf_count(&rb), 0);
    SCL_EXPECT_TRUE(tr, scl_cringbuf_empty(&rb));
    scl_cringbuf_destroy(alloc, &rb);
}

static void test_cringbuf_push_pop(scl_test_runner_t *tr) {
    scl_test_group("CRingbuf: push and pop");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_ringbuf_t rb;
    scl_cringbuf_init(alloc, &rb, sizeof(int), CAPACITY);

    int v = 77;
    SCL_EXPECT_OK(tr, scl_cringbuf_push(&rb, &v));
    SCL_EXPECT_EQ_SZ(tr, scl_cringbuf_count(&rb), 1);
    SCL_EXPECT_FALSE(tr, scl_cringbuf_empty(&rb));

    int out = 0;
    SCL_EXPECT_OK(tr, scl_cringbuf_pop(&rb, &out));
    SCL_EXPECT_EQ_I(tr, out, 77);
    SCL_EXPECT_TRUE(tr, scl_cringbuf_empty(&rb));

    scl_cringbuf_destroy(alloc, &rb);
}

static void test_cringbuf_fifo_order(scl_test_runner_t *tr) {
    scl_test_group("CRingbuf: FIFO ordering");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_ringbuf_t rb;
    scl_cringbuf_init(alloc, &rb, sizeof(int), CAPACITY);

    for (int i = 0; i < 10; i++)
        scl_cringbuf_push(&rb, &i);

    for (int i = 0; i < 10; i++) {
        int out = -1;
        scl_cringbuf_pop(&rb, &out);
        SCL_EXPECT_EQ_I(tr, out, i);
    }
    scl_cringbuf_destroy(alloc, &rb);
}

static void test_cringbuf_peek(scl_test_runner_t *tr) {
    scl_test_group("CRingbuf: peek");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_ringbuf_t rb;
    scl_cringbuf_init(alloc, &rb, sizeof(int), CAPACITY);

    int a = 10, b = 20;
    scl_cringbuf_push(&rb, &a);
    scl_cringbuf_push(&rb, &b);

    int out = 0;
    SCL_EXPECT_OK(tr, scl_cringbuf_peek(&rb, 0, &out));
    SCL_EXPECT_EQ_I(tr, out, 10);
    SCL_EXPECT_OK(tr, scl_cringbuf_peek(&rb, 1, &out));
    SCL_EXPECT_EQ_I(tr, out, 20);
    SCL_EXPECT_EQ_SZ(tr, scl_cringbuf_count(&rb), 2);

    scl_cringbuf_destroy(alloc, &rb);
}

static void test_cringbuf_full(scl_test_runner_t *tr) {
    scl_test_group("CRingbuf: push to full returns error");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_ringbuf_t rb;
    scl_cringbuf_init(alloc, &rb, sizeof(int), 4);

    for (int i = 0; i < 4; i++) {
        scl_error_t e = scl_cringbuf_push(&rb, &i);
        SCL_EXPECT_OK(tr, e);
    }
    int v = 99;
    scl_error_t e = scl_cringbuf_push(&rb, &v);
    SCL_EXPECT_TRUE(tr, e != SCL_OK);

    scl_cringbuf_destroy(alloc, &rb);
}

static void test_cringbuf_empty_pop(scl_test_runner_t *tr) {
    scl_test_group("CRingbuf: pop from empty returns error");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_ringbuf_t rb;
    scl_cringbuf_init(alloc, &rb, sizeof(int), 4);
    int out;
    scl_error_t e = scl_cringbuf_pop(&rb, &out);
    SCL_EXPECT_TRUE(tr, e != SCL_OK);
    scl_cringbuf_destroy(alloc, &rb);
}

typedef struct { scl_concurrent_ringbuf_t *rb; } ringbuf_arg_t;

static atomic_int rb_consumed;

static void *rb_producer(void *arg) {
    scl_concurrent_ringbuf_t *rb = ((ringbuf_arg_t *)arg)->rb;
    for (int i = 0; i < N_ITEMS; i++) {
        while (scl_cringbuf_push(rb, &i) != SCL_OK)
            ;
    }
    return NULL;
}

static void *rb_consumer(void *arg) {
    scl_concurrent_ringbuf_t *rb = ((ringbuf_arg_t *)arg)->rb;
    int out;
    while (atomic_load(&rb_consumed) < N_ITEMS) {
        if (scl_cringbuf_pop(rb, &out) == SCL_OK)
            atomic_fetch_add(&rb_consumed, 1);
    }
    return NULL;
}

static void test_cringbuf_spsc(scl_test_runner_t *tr) {
    scl_test_group("CRingbuf: SPSC throughput");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_ringbuf_t rb;
    scl_cringbuf_init(alloc, &rb, sizeof(int), CAPACITY);

    atomic_init(&rb_consumed, 0);
    ringbuf_arg_t arg = {.rb = &rb};
    pthread_t prod, cons;
    pthread_create(&prod, NULL, rb_producer, &arg);
    pthread_create(&cons, NULL, rb_consumer, &arg);
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    SCL_EXPECT_EQ_I(tr, atomic_load(&rb_consumed), N_ITEMS);
    scl_cringbuf_destroy(alloc, &rb);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_cringbuf_init_destroy(&tr);
    test_cringbuf_push_pop(&tr);
    test_cringbuf_fifo_order(&tr);
    test_cringbuf_peek(&tr);
    test_cringbuf_full(&tr);
    test_cringbuf_empty_pop(&tr);
    test_cringbuf_spsc(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
