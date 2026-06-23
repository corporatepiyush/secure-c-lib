#include "scl_test.h"
#include "scl_concurrent_heap.h"
#include <pthread.h>
#include <stdatomic.h>

#define NTHREADS 4
#define OPS_PER_THREAD 500

static int int_cmp_min(const void *a, const void *b) {
    int va = *(const int *)a, vb = *(const int *)b;
    return (va > vb) - (va < vb);
}

static void test_cheap_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("CHeap: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_heap_t h;
    scl_error_t err = scl_cheap_init(alloc, &h, sizeof(int), 16, int_cmp_min);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_cheap_count(&h), 0);
    SCL_EXPECT_TRUE(tr, scl_cheap_empty(&h));
    scl_cheap_destroy(alloc, &h);
}

static void test_cheap_push_pop(scl_test_runner_t *tr) {
    scl_test_group("CHeap: push and pop");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_heap_t h;
    scl_cheap_init(alloc, &h, sizeof(int), 16, int_cmp_min);

    int v = 42;
    SCL_EXPECT_OK(tr, scl_cheap_push(alloc, &h, &v));
    SCL_EXPECT_EQ_SZ(tr, scl_cheap_count(&h), 1);

    int out = 0;
    SCL_EXPECT_OK(tr, scl_cheap_pop(&h, &out));
    SCL_EXPECT_EQ_I(tr, out, 42);
    SCL_EXPECT_TRUE(tr, scl_cheap_empty(&h));

    scl_cheap_destroy(alloc, &h);
}

static void test_cheap_peek(scl_test_runner_t *tr) {
    scl_test_group("CHeap: peek");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_heap_t h;
    scl_cheap_init(alloc, &h, sizeof(int), 16, int_cmp_min);

    int v = 99;
    scl_cheap_push(alloc, &h, &v);
    int out = 0;
    SCL_EXPECT_OK(tr, scl_cheap_peek(&h, &out));
    SCL_EXPECT_EQ_I(tr, out, 99);
    SCL_EXPECT_EQ_SZ(tr, scl_cheap_count(&h), 1);

    scl_cheap_destroy(alloc, &h);
}

static void test_cheap_ordering(scl_test_runner_t *tr) {
    scl_test_group("CHeap: min-heap ordering");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_heap_t h;
    scl_cheap_init(alloc, &h, sizeof(int), 16, int_cmp_min);

    int vals[] = {5, 3, 7, 1, 9, 2, 8};
    for (int i = 0; i < 7; i++)
        scl_cheap_push(alloc, &h, &vals[i]);

    int sorted[] = {1, 2, 3, 5, 7, 8, 9};
    for (int i = 0; i < 7; i++) {
        int out;
        scl_cheap_pop(&h, &out);
        SCL_EXPECT_EQ_I(tr, out, sorted[i]);
    }
    scl_cheap_destroy(alloc, &h);
}

static void test_cheap_empty_pop(scl_test_runner_t *tr) {
    scl_test_group("CHeap: pop from empty returns error");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_heap_t h;
    scl_cheap_init(alloc, &h, sizeof(int), 16, int_cmp_min);
    int out;
    SCL_EXPECT_TRUE(tr, scl_cheap_pop(&h, &out) != SCL_OK);
    scl_cheap_destroy(alloc, &h);
}

typedef struct { scl_concurrent_heap_t *h; int base; } cheap_arg_t;

static atomic_int cheap_consumed;

static void *cheap_push_thread(void *arg) {
    cheap_arg_t *a = arg;
    scl_allocator_t *alloc = scl_allocator_default();
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int v = a->base + i;
        while (scl_cheap_push(alloc, a->h, &v) != SCL_OK)
            ;
    }
    return NULL;
}

static void *cheap_pop_thread(void *arg) {
    scl_concurrent_heap_t *h = arg;
    int total = NTHREADS * OPS_PER_THREAD;
    while (atomic_load(&cheap_consumed) < total) {
        int out;
        if (scl_cheap_pop(h, &out) == SCL_OK)
            atomic_fetch_add(&cheap_consumed, 1);
    }
    return NULL;
}

static void test_cheap_concurrent(scl_test_runner_t *tr) {
    scl_test_group("CHeap: concurrent push/pop");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_heap_t h;
    scl_cheap_init(alloc, &h, sizeof(int), NTHREADS * OPS_PER_THREAD + 64, int_cmp_min);

    atomic_init(&cheap_consumed, 0);

    pthread_t pushers[NTHREADS], popper;
    cheap_arg_t args[NTHREADS];
    for (int i = 0; i < NTHREADS; i++) {
        args[i] = (cheap_arg_t){.h = &h, .base = i * OPS_PER_THREAD};
        pthread_create(&pushers[i], NULL, cheap_push_thread, &args[i]);
    }
    pthread_create(&popper, NULL, cheap_pop_thread, &h);

    for (int i = 0; i < NTHREADS; i++)
        pthread_join(pushers[i], NULL);
    pthread_join(popper, NULL);

    SCL_EXPECT_EQ_I(tr, atomic_load(&cheap_consumed), NTHREADS * OPS_PER_THREAD);
    SCL_EXPECT_TRUE(tr, scl_cheap_empty(&h));

    scl_cheap_destroy(alloc, &h);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_cheap_init_destroy(&tr);
    test_cheap_push_pop(&tr);
    test_cheap_peek(&tr);
    test_cheap_ordering(&tr);
    test_cheap_empty_pop(&tr);
    test_cheap_concurrent(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
