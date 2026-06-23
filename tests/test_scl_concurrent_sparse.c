#include "scl_test.h"
#include "scl_concurrent_sparse.h"
#include <pthread.h>
#include <stdatomic.h>

#define NTHREADS 4
#define N 32

static void combine_min(void *out, const void *a, const void *b) {
    int va = *(const int *)a, vb = *(const int *)b;
    *(int *)out = va < vb ? va : vb;
}

static void test_csparse_init_destroy(scl_test_runner_t *tr) {
    scl_test_group("CSparse: init and destroy");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_sparse_t st;
    int data[] = {3, 2, -1, 6, 5, 4, -2, 3};
    scl_error_t err = scl_csparse_init(alloc, &st, 8, sizeof(int), data, combine_min);
    SCL_EXPECT_OK(tr, err);
    scl_csparse_destroy(alloc, &st);
}

static void test_csparse_query(scl_test_runner_t *tr) {
    scl_test_group("CSparse: query");
    scl_allocator_t *alloc = scl_allocator_default();
    scl_concurrent_sparse_t st;
    int data[] = {3, 2, 1, 6, 5, 4, 2, 3};
    scl_csparse_init(alloc, &st, 8, sizeof(int), data, combine_min);

    int result;
    SCL_EXPECT_OK(tr, scl_csparse_query(&st, 2, 4, &result));
    SCL_EXPECT_EQ_I(tr, result, 1);

    scl_csparse_destroy(alloc, &st);
}

typedef struct { scl_concurrent_sparse_t *st; } csparse_arg_t;

static void *csparse_query_thread(void *arg) {
    scl_concurrent_sparse_t *st = arg;
    for (int i = 0; i < 100; i++) {
        size_t l = (size_t)(i % N);
        size_t r = l + (size_t)((i + 1) % (N - l));
        if (r > l) {
            int result;
            scl_csparse_query(st, l, r, &result);
        }
    }
    return NULL;
}

static void test_csparse_concurrent_query(scl_test_runner_t *tr) {
    (void)tr;
    scl_test_group("CSparse: concurrent queries");
    scl_allocator_t *alloc = scl_allocator_default();
    int data[N];
    for (size_t i = 0; i < N; i++) data[i] = (int)i;
    scl_concurrent_sparse_t st;
    scl_csparse_init(alloc, &st, N, sizeof(int), data, combine_min);

    pthread_t threads[NTHREADS];
    for (int i = 0; i < NTHREADS; i++)
        pthread_create(&threads[i], NULL, csparse_query_thread, &st);
    for (int i = 0; i < NTHREADS; i++)
        pthread_join(threads[i], NULL);

    scl_csparse_destroy(alloc, &st);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_csparse_init_destroy(&tr);
    test_csparse_query(&tr);
    test_csparse_concurrent_query(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
