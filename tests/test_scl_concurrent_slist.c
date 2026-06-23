#include "scl_test.h"
#include "scl_concurrent_slist.h"
#include "scl_pthread.h"
#include "scl_atomic.h"
#include <sched.h>

#define NTHREADS 4
#define OPS_PER_THREAD 1000

static void test_cslist_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("CSList: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_slist_t l;
    scl_error_t err = scl_cslist_init(alloc, &l, sizeof(int));
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_cslist_count(&l), 0);
    SCL_EXPECT_TRUE(tr, scl_cslist_empty(&l));
    scl_cslist_destroy(alloc, &l);
}

static void test_cslist_push_pop_front(scl_test_runner_t *tr) {
    scl_test_group("CSList: push_front and pop_front");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_slist_t l;
    scl_cslist_init(alloc, &l, sizeof(int));

    int v = 42;
    SCL_EXPECT_OK(tr, scl_cslist_push_front(alloc, &l, &v));
    SCL_EXPECT_EQ_SZ(tr, scl_cslist_count(&l), 1);
    SCL_EXPECT_FALSE(tr, scl_cslist_empty(&l));

    int out = 0;
    SCL_EXPECT_OK(tr, scl_cslist_pop_front(alloc, &l, &out));
    SCL_EXPECT_EQ_I(tr, out, 42);
    SCL_EXPECT_TRUE(tr, scl_cslist_empty(&l));

    scl_cslist_destroy(alloc, &l);
}

static void test_cslist_lifo_order(scl_test_runner_t *tr) {
    scl_test_group("CSList: LIFO ordering");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_slist_t l;
    scl_cslist_init(alloc, &l, sizeof(int));

    for (int i = 0; i < 8; i++)
        scl_cslist_push_front(alloc, &l, &i);

    for (int i = 7; i >= 0; i--) {
        int out = -1;
        scl_cslist_pop_front(alloc, &l, &out);
        SCL_EXPECT_EQ_I(tr, out, i);
    }
    scl_cslist_destroy(alloc, &l);
}

static void test_cslist_empty_pop(scl_test_runner_t *tr) {
    scl_test_group("CSList: pop from empty returns error");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_slist_t l;
    scl_cslist_init(alloc, &l, sizeof(int));
    int out;
    SCL_EXPECT_TRUE(tr, scl_cslist_pop_front(alloc, &l, &out) != SCL_OK);
    scl_cslist_destroy(alloc, &l);
}

static atomic_int cslist_push_count;
static atomic_int cslist_pop_count;

static void *cslist_push_thread(void *arg) {
    scl_concurrent_slist_t *l = arg;
    scl_allocator_t *alloc = scl_allocator_default();
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        while (scl_cslist_push_front(alloc, l, &i) != SCL_OK)
            sched_yield();
        atomic_fetch_add(&cslist_push_count, 1);
    }
    return NULL;
}

static void *cslist_pop_thread(void *arg) {
    scl_concurrent_slist_t *l = arg;
    scl_allocator_t *alloc = scl_allocator_default();
    int total = NTHREADS * OPS_PER_THREAD;
    while (atomic_load(&cslist_pop_count) < total) {
        int out;
        if (scl_cslist_pop_front(alloc, l, &out) == SCL_OK)
            atomic_fetch_add(&cslist_pop_count, 1);
        else
            sched_yield();
    }
    return NULL;
}

static void test_cslist_concurrent(scl_test_runner_t *tr) {
    scl_test_group("CSList: concurrent push/pop");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_slist_t l;
    scl_cslist_init(alloc, &l, sizeof(int));

    atomic_init(&cslist_push_count, 0);
    atomic_init(&cslist_pop_count, 0);

    pthread_t pushers[NTHREADS], popper;
    for (int i = 0; i < NTHREADS; i++)
        pthread_create(&pushers[i], NULL, cslist_push_thread, &l);
    pthread_create(&popper, NULL, cslist_pop_thread, &l);

    for (int i = 0; i < NTHREADS; i++)
        pthread_join(pushers[i], NULL);
    pthread_join(popper, NULL);

    SCL_EXPECT_EQ_I(tr, atomic_load(&cslist_pop_count), NTHREADS * OPS_PER_THREAD);
    SCL_EXPECT_TRUE(tr, scl_cslist_empty(&l));

    scl_cslist_destroy(alloc, &l);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_cslist_init_destroy(&tr);
    test_cslist_push_pop_front(&tr);
    test_cslist_lifo_order(&tr);
    test_cslist_empty_pop(&tr);
    test_cslist_concurrent(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
