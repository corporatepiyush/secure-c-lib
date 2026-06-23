#include "scl_test.h"
#include "scl_concurrent_deque.h"
#include "scl_pthread.h"
#include "scl_atomic.h"

#define NTHREADS 4
#define OPS_PER_THREAD 500

static void test_cdeque_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("CDeque: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_deque_t d;
    scl_error_t err = scl_cdeque_init(alloc, &d, sizeof(int), 16);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_cdeque_count(&d), 0);
    SCL_EXPECT_TRUE(tr, scl_cdeque_empty(&d));
    scl_cdeque_destroy(alloc, &d);
}

static void test_cdeque_push_pop_front(scl_test_runner_t *tr) {
    scl_test_group("CDeque: push_front and pop_front");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_deque_t d;
    scl_cdeque_init(alloc, &d, sizeof(int), 16);

    int v = 42;
    SCL_EXPECT_OK(tr, scl_cdeque_push_front(alloc, &d, &v));
    SCL_EXPECT_EQ_SZ(tr, scl_cdeque_count(&d), 1);

    int out = 0;
    SCL_EXPECT_OK(tr, scl_cdeque_pop_front(&d, &out));
    SCL_EXPECT_EQ_I(tr, out, 42);
    SCL_EXPECT_TRUE(tr, scl_cdeque_empty(&d));

    scl_cdeque_destroy(alloc, &d);
}

static void test_cdeque_push_pop_back(scl_test_runner_t *tr) {
    scl_test_group("CDeque: push_back and pop_back");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_deque_t d;
    scl_cdeque_init(alloc, &d, sizeof(int), 16);

    int v = 99;
    SCL_EXPECT_OK(tr, scl_cdeque_push_back(alloc, &d, &v));
    SCL_EXPECT_EQ_SZ(tr, scl_cdeque_count(&d), 1);

    int out = 0;
    SCL_EXPECT_OK(tr, scl_cdeque_pop_back(&d, &out));
    SCL_EXPECT_EQ_I(tr, out, 99);

    scl_cdeque_destroy(alloc, &d);
}

static void test_cdeque_ordering(scl_test_runner_t *tr) {
    scl_test_group("CDeque: ordering");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_deque_t d;
    scl_cdeque_init(alloc, &d, sizeof(int), 16);

    for (int i = 0; i < 5; i++)
        scl_cdeque_push_back(alloc, &d, &i);
    for (int i = 0; i < 5; i++) {
        int out = -1;
        scl_cdeque_pop_front(&d, &out);
        SCL_EXPECT_EQ_I(tr, out, i);
    }
    scl_cdeque_destroy(alloc, &d);
}

static void test_cdeque_empty_pop(scl_test_runner_t *tr) {
    scl_test_group("CDeque: pop from empty returns error");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_deque_t d;
    scl_cdeque_init(alloc, &d, sizeof(int), 16);
    int out;
    SCL_EXPECT_TRUE(tr, scl_cdeque_pop_front(&d, &out) != SCL_OK);
    SCL_EXPECT_TRUE(tr, scl_cdeque_pop_back(&d, &out) != SCL_OK);
    scl_cdeque_destroy(alloc, &d);
}

typedef struct { scl_concurrent_deque_t *d; int base; } cdeque_arg_t;

static atomic_int cdeque_consumed;

static void *cdeque_push_thread(void *arg) {
    cdeque_arg_t *a = arg;
    scl_allocator_t *alloc = scl_allocator_default();
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int v = a->base + i;
        while (scl_cdeque_push_back(alloc, a->d, &v) != SCL_OK)
            ;
    }
    return NULL;
}

static void *cdeque_pop_thread(void *arg) {
    scl_concurrent_deque_t *d = arg;
    int total = NTHREADS * OPS_PER_THREAD;
    while (atomic_load(&cdeque_consumed) < total) {
        int out;
        if (scl_cdeque_pop_front(d, &out) == SCL_OK)
            atomic_fetch_add(&cdeque_consumed, 1);
    }
    return NULL;
}

static void test_cdeque_concurrent(scl_test_runner_t *tr) {
    scl_test_group("CDeque: concurrent push_back/pop_front");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_deque_t d;
    scl_cdeque_init(alloc, &d, sizeof(int), NTHREADS * OPS_PER_THREAD + 64);

    atomic_init(&cdeque_consumed, 0);

    pthread_t pushers[NTHREADS], popper;
    cdeque_arg_t args[NTHREADS];
    for (int i = 0; i < NTHREADS; i++) {
        args[i] = (cdeque_arg_t){.d = &d, .base = i * OPS_PER_THREAD};
        pthread_create(&pushers[i], NULL, cdeque_push_thread, &args[i]);
    }
    pthread_create(&popper, NULL, cdeque_pop_thread, &d);

    for (int i = 0; i < NTHREADS; i++)
        pthread_join(pushers[i], NULL);
    pthread_join(popper, NULL);

    SCL_EXPECT_EQ_I(tr, atomic_load(&cdeque_consumed), NTHREADS * OPS_PER_THREAD);
    scl_cdeque_destroy(alloc, &d);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_cdeque_init_destroy(&tr);
    test_cdeque_push_pop_front(&tr);
    test_cdeque_push_pop_back(&tr);
    test_cdeque_ordering(&tr);
    test_cdeque_empty_pop(&tr);
    test_cdeque_concurrent(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
