#include "scl_atomic.h"
#include "scl_concurrent_fenwick.h"
#include "scl_pthread.h"
#include "scl_test.h"

#define NTHREADS 4
#define N 16

static void add_int(void *out, const void *a, const void *b) {
  *(int *)out = *(const int *)a + *(const int *)b;
}

static void sub_int(void *out, const void *a, const void *b) {
  *(int *)out = *(const int *)a - *(const int *)b;
}

static void test_cfenwick_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CFenwick: init and destroy");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_fenwick_t fw;
  int data[] = {1, 2, 3, 4, 5, 6, 7, 8};
  scl_error_t err =
      scl_cfenwick_init(alloc, &fw, 8, sizeof(int), data, add_int, sub_int);
  SCL_EXPECT_OK(tr, err);
  scl_cfenwick_destroy(alloc, &fw);
  TEST_TRACE_END();
}

static void test_cfenwick_prefix(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CFenwick: prefix query");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_fenwick_t fw;
  int data[] = {1, 2, 3, 4, 5, 6, 7, 8};
  scl_cfenwick_init(alloc, &fw, 8, sizeof(int), data, add_int, sub_int);

  int result;
  SCL_EXPECT_OK(tr, scl_cfenwick_prefix(&fw, 3, &result));
  SCL_EXPECT_EQ_I(tr, result, 10);

  scl_cfenwick_destroy(alloc, &fw);
  TEST_TRACE_END();
}

static void test_cfenwick_update(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CFenwick: update");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_fenwick_t fw;
  int data[] = {1, 2, 3, 4, 5, 6, 7, 8};
  scl_cfenwick_init(alloc, &fw, 8, sizeof(int), data, add_int, sub_int);

  int delta = 5;
  SCL_EXPECT_OK(tr, scl_cfenwick_update(alloc, &fw, 2, &delta));

  int result;
  scl_cfenwick_prefix(&fw, 2, &result);
  SCL_EXPECT_EQ_I(tr, result, 1 + 2 + 8);

  scl_cfenwick_destroy(alloc, &fw);
  TEST_TRACE_END();
}

typedef struct {
  scl_concurrent_fenwick_t *fw;
} cfenwick_arg_t;

static atomic_int cfenwick_updated;

static void *cfenwick_update_thread(void *arg) {
  scl_concurrent_fenwick_t *fw = arg;
  scl_allocator_t *alloc = scl_allocator_default();
  for (size_t i = 0; i < N; i++) {
    int delta = 1;
    if (scl_cfenwick_update(alloc, fw, i, &delta) == SCL_OK)
      atomic_fetch_add(&cfenwick_updated, 1);
  }
  return NULL;
}

static void test_cfenwick_concurrent_update(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CFenwick: concurrent updates");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_fenwick_t fw;
  int data[N];
  for (size_t i = 0; i < N; i++)
    data[i] = 0;
  scl_cfenwick_init(alloc, &fw, N, sizeof(int), data, add_int, sub_int);

  atomic_init(&cfenwick_updated, 0);

  pthread_t threads[NTHREADS];
  for (int i = 0; i < NTHREADS; i++)
    pthread_create(&threads[i], NULL, cfenwick_update_thread, &fw);

  for (int i = 0; i < NTHREADS; i++)
    pthread_join(threads[i], NULL);

  SCL_EXPECT_EQ_I(tr, atomic_load(&cfenwick_updated), NTHREADS * N);

  int total;
  scl_cfenwick_prefix(&fw, N - 1, &total);
  SCL_EXPECT_EQ_I(tr, total, NTHREADS * N);

  scl_cfenwick_destroy(alloc, &fw);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_cfenwick_init_destroy(&tr);
  test_cfenwick_prefix(&tr);
  test_cfenwick_update(&tr);
  test_cfenwick_concurrent_update(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
