#include "scl_test.h"
#include "scl_concurrent_array.h"
#include "scl_pthread.h"
#include "scl_atomic.h"

#define NTHREADS 4
#define OPS_PER_THREAD 1000

static void test_carray_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("CArray: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_array_t a;
    scl_error_t err = scl_carray_init(alloc, &a, sizeof(int), 16);
    SCL_EXPECT_OK(tr, err);
    SCL_EXPECT_EQ_SZ(tr, scl_carray_count(&a), 0);
    SCL_EXPECT_TRUE(tr, scl_carray_empty(&a));
    scl_carray_destroy(alloc, &a);
}

static void test_carray_push_pop(scl_test_runner_t *tr) {
    scl_test_group("CArray: push and pop");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_array_t a;
    scl_carray_init(alloc, &a, sizeof(int), 16);

    int v = 42;
    SCL_EXPECT_OK(tr, scl_carray_push(alloc, &a, &v));
    SCL_EXPECT_EQ_SZ(tr, scl_carray_count(&a), 1);
    SCL_EXPECT_FALSE(tr, scl_carray_empty(&a));

    int out = 0;
    SCL_EXPECT_OK(tr, scl_carray_pop(&a, &out));
    SCL_EXPECT_EQ_I(tr, out, 42);
    SCL_EXPECT_TRUE(tr, scl_carray_empty(&a));

    scl_carray_destroy(alloc, &a);
}

static void test_carray_get_set(scl_test_runner_t *tr) {
    scl_test_group("CArray: get and set");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_array_t a;
    scl_carray_init(alloc, &a, sizeof(int), 16);

    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
        scl_carray_push(alloc, &a, &vals[i]);

    int out;
    SCL_EXPECT_OK(tr, scl_carray_get(&a, 1, &out));
    SCL_EXPECT_EQ_I(tr, out, 20);

    int nv = 25;
    SCL_EXPECT_OK(tr, scl_carray_set(&a, 1, &nv));
    SCL_EXPECT_OK(tr, scl_carray_get(&a, 1, &out));
    SCL_EXPECT_EQ_I(tr, out, 25);

    scl_carray_destroy(alloc, &a);
}

static void test_carray_lifo_order(scl_test_runner_t *tr) {
    scl_test_group("CArray: LIFO ordering");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_array_t a;
    scl_carray_init(alloc, &a, sizeof(int), 16);

    for (int i = 0; i < 8; i++)
        scl_carray_push(alloc, &a, &i);

    for (int i = 7; i >= 0; i--) {
        int out = -1;
        scl_carray_pop(&a, &out);
        SCL_EXPECT_EQ_I(tr, out, i);
    }
    scl_carray_destroy(alloc, &a);
}

static void test_carray_empty_pop(scl_test_runner_t *tr) {
    scl_test_group("CArray: pop from empty returns error");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_array_t a;
    scl_carray_init(alloc, &a, sizeof(int), 16);
    int out;
    SCL_EXPECT_TRUE(tr, scl_carray_pop(&a, &out) != SCL_OK);
    scl_carray_destroy(alloc, &a);
}

static atomic_int carray_push_count;
static atomic_int carray_pop_count;

static void *carray_push_thread(void *arg) {
    scl_concurrent_array_t *a = arg;
    scl_allocator_t *alloc = scl_allocator_default();
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        while (scl_carray_push(alloc, a, &i) != SCL_OK)
            ;
        atomic_fetch_add(&carray_push_count, 1);
    }
    return NULL;
}

static void *carray_pop_thread(void *arg) {
    scl_concurrent_array_t *a = arg;
    int total = NTHREADS * OPS_PER_THREAD;
    while (atomic_load(&carray_pop_count) < total) {
        int out;
        if (scl_carray_pop(a, &out) == SCL_OK)
            atomic_fetch_add(&carray_pop_count, 1);
    }
    return NULL;
}

static void test_carray_concurrent(scl_test_runner_t *tr) {
    scl_test_group("CArray: concurrent push/pop");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_array_t a;
    scl_carray_init(alloc, &a, sizeof(int), NTHREADS * OPS_PER_THREAD + 64);

    atomic_init(&carray_push_count, 0);
    atomic_init(&carray_pop_count, 0);

    pthread_t pushers[NTHREADS], popper;
    for (int i = 0; i < NTHREADS; i++)
        pthread_create(&pushers[i], NULL, carray_push_thread, &a);
    pthread_create(&popper, NULL, carray_pop_thread, &a);

    for (int i = 0; i < NTHREADS; i++)
        pthread_join(pushers[i], NULL);
    pthread_join(popper, NULL);

    SCL_EXPECT_EQ_I(tr, atomic_load(&carray_pop_count), NTHREADS * OPS_PER_THREAD);
    SCL_EXPECT_TRUE(tr, scl_carray_empty(&a));

    scl_carray_destroy(alloc, &a);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_carray_init_destroy(&tr);
    test_carray_push_pop(&tr);
    test_carray_get_set(&tr);
    test_carray_lifo_order(&tr);
    test_carray_empty_pop(&tr);
    test_carray_concurrent(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
