#include "scl_test.h"
#include "scl_concurrent_dlist.h"
#include <pthread.h>
#include <stdatomic.h>

#define NTHREADS 4
#define OPS_PER_THREAD 500

static void test_cdlist_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("CDList: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_dlist_t l;
    scl_error_t err = scl_cdlist_init(alloc, &l, sizeof(int));
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_cdlist_count(&l), 0);
    SCL_EXPECT_TRUE(tr, scl_cdlist_empty(&l));
    scl_cdlist_destroy(alloc, &l);
}

static void test_cdlist_push_pop_front(scl_test_runner_t *tr) {
    scl_test_group("CDList: push_front and pop_front");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_dlist_t l;
    scl_cdlist_init(alloc, &l, sizeof(int));

    int v = 42;
    SCL_EXPECT_OK(tr, scl_cdlist_push_front(alloc, &l, &v));
    SCL_EXPECT_EQ_SZ(tr, scl_cdlist_count(&l), 1);

    int out = 0;
    SCL_EXPECT_OK(tr, scl_cdlist_pop_front(alloc, &l, &out));
    SCL_EXPECT_EQ_I(tr, out, 42);
    SCL_EXPECT_TRUE(tr, scl_cdlist_empty(&l));

    scl_cdlist_destroy(alloc, &l);
}

static void test_cdlist_push_pop_back(scl_test_runner_t *tr) {
    scl_test_group("CDList: push_back and pop_back");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_dlist_t l;
    scl_cdlist_init(alloc, &l, sizeof(int));

    int v = 99;
    SCL_EXPECT_OK(tr, scl_cdlist_push_back(alloc, &l, &v));
    SCL_EXPECT_EQ_SZ(tr, scl_cdlist_count(&l), 1);

    int out = 0;
    SCL_EXPECT_OK(tr, scl_cdlist_pop_back(alloc, &l, &out));
    SCL_EXPECT_EQ_I(tr, out, 99);

    scl_cdlist_destroy(alloc, &l);
}

static void test_cdlist_insert_remove_at(scl_test_runner_t *tr) {
    scl_test_group("CDList: insert_at and remove_at");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_dlist_t l;
    scl_cdlist_init(alloc, &l, sizeof(int));

    int v = 7;
    SCL_EXPECT_OK(tr, scl_cdlist_insert_at(alloc, &l, 0, &v));
    SCL_EXPECT_EQ_SZ(tr, scl_cdlist_count(&l), 1);

    int out = 0;
    SCL_EXPECT_OK(tr, scl_cdlist_remove_at(alloc, &l, 0, &out));
    SCL_EXPECT_EQ_I(tr, out, 7);
    SCL_EXPECT_TRUE(tr, scl_cdlist_empty(&l));

    scl_cdlist_destroy(alloc, &l);
}

static void test_cdlist_ordering(scl_test_runner_t *tr) {
    scl_test_group("CDList: ordering");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_dlist_t l;
    scl_cdlist_init(alloc, &l, sizeof(int));

    for (int i = 0; i < 5; i++)
        scl_cdlist_push_back(alloc, &l, &i);
    for (int i = 0; i < 5; i++) {
        int out = -1;
        scl_cdlist_pop_front(alloc, &l, &out);
        SCL_EXPECT_EQ_I(tr, out, i);
    }
    scl_cdlist_destroy(alloc, &l);
}

static void test_cdlist_empty_pop(scl_test_runner_t *tr) {
    scl_test_group("CDList: pop from empty returns error");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_dlist_t l;
    scl_cdlist_init(alloc, &l, sizeof(int));
    int out;
    SCL_EXPECT_TRUE(tr, scl_cdlist_pop_front(alloc, &l, &out) != SCL_OK);
    SCL_EXPECT_TRUE(tr, scl_cdlist_pop_back(alloc, &l, &out) != SCL_OK);
    scl_cdlist_destroy(alloc, &l);
}

typedef struct { scl_concurrent_dlist_t *l; int base; } cdlist_arg_t;

static atomic_int cdlist_consumed;

static void *cdlist_push_thread(void *arg) {
    cdlist_arg_t *a = arg;
    scl_allocator_t *alloc = scl_allocator_default();
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int v = a->base + i;
        scl_cdlist_push_back(alloc, a->l, &v);
    }
    return NULL;
}

static void *cdlist_pop_thread(void *arg) {
    scl_concurrent_dlist_t *l = arg;
    scl_allocator_t *alloc = scl_allocator_default();
    int total = NTHREADS * OPS_PER_THREAD;
    while (atomic_load(&cdlist_consumed) < total) {
        int out;
        if (scl_cdlist_pop_front(alloc, l, &out) == SCL_OK)
            atomic_fetch_add(&cdlist_consumed, 1);
    }
    return NULL;
}

static void test_cdlist_concurrent(scl_test_runner_t *tr) {
    scl_test_group("CDList: concurrent push/pop");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_dlist_t l;
    scl_cdlist_init(alloc, &l, sizeof(int));

    atomic_init(&cdlist_consumed, 0);

    pthread_t pushers[NTHREADS], popper;
    cdlist_arg_t args[NTHREADS];
    for (int i = 0; i < NTHREADS; i++) {
        args[i] = (cdlist_arg_t){.l = &l, .base = i * OPS_PER_THREAD};
        pthread_create(&pushers[i], NULL, cdlist_push_thread, &args[i]);
    }
    pthread_create(&popper, NULL, cdlist_pop_thread, &l);

    for (int i = 0; i < NTHREADS; i++)
        pthread_join(pushers[i], NULL);
    pthread_join(popper, NULL);

    SCL_EXPECT_EQ_I(tr, atomic_load(&cdlist_consumed), NTHREADS * OPS_PER_THREAD);
    scl_cdlist_destroy(alloc, &l);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_cdlist_init_destroy(&tr);
    test_cdlist_push_pop_front(&tr);
    test_cdlist_push_pop_back(&tr);
    test_cdlist_insert_remove_at(&tr);
    test_cdlist_ordering(&tr);
    test_cdlist_empty_pop(&tr);
    test_cdlist_concurrent(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
