#include "scl_atomic.h"
#include "scl_concurrent_segtree.h"
#include "scl_pthread.h"
#include "scl_test.h"

#define NTHREADS 4
#define N 16

static void combine_min(void *out, const void *a, const void *b) {
  int va = *(const int *)a, vb = *(const int *)b;
  *(int *)out = va < vb ? va : vb;
}

static void test_csegtree_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSegTree: init and destroy");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_segtree_t t;
  int data[] = {3, 2, -1, 6, 5, 4, -2, 3};
  scl_error_t err =
      scl_csegtree_init(alloc, &t, 8, sizeof(int), data, combine_min);
  SCL_EXPECT_OK(tr, err);
  scl_csegtree_destroy(alloc, &t);
  TEST_TRACE_END();
}

static void test_csegtree_query(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSegTree: query");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_segtree_t t;
  int data[] = {3, 2, 1, 6, 5, 4, 2, 3};
  scl_csegtree_init(alloc, &t, 8, sizeof(int), data, combine_min);

  int result;
  SCL_EXPECT_OK(tr, scl_csegtree_query(&t, 2, 4, &result));
  SCL_EXPECT_EQ_I(tr, result, 1);

  scl_csegtree_destroy(alloc, &t);
  TEST_TRACE_END();
}

static void test_csegtree_update(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSegTree: update");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_segtree_t t;
  int data[] = {3, 2, 1, 6, 5, 4, 2, 3};
  scl_csegtree_init(alloc, &t, 8, sizeof(int), data, combine_min);

  int newval = 0;
  SCL_EXPECT_OK(tr, scl_csegtree_update(alloc, &t, 3, &newval));

  int result;
  scl_csegtree_query(&t, 2, 4, &result);
  SCL_EXPECT_EQ_I(tr, result, 0);

  scl_csegtree_destroy(alloc, &t);
  TEST_TRACE_END();
}

typedef struct {
  scl_concurrent_segtree_t *t;
} csegtree_arg_t;

static atomic_int csegtree_updates;

static void *csegtree_update_thread(void *arg) {
  scl_concurrent_segtree_t *t = arg;
  scl_allocator_t *alloc = scl_allocator_default();
  for (size_t i = 0; i < N; i++) {
    int v = -1;
    if (scl_csegtree_update(alloc, t, i, &v) == SCL_OK)
      atomic_fetch_add(&csegtree_updates, 1);
  }
  return NULL;
}

static void test_csegtree_concurrent_update(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSegTree: concurrent updates");
  scl_allocator_t *alloc = scl_allocator_default();
  int data[N];
  for (size_t i = 0; i < N; i++)
    data[i] = (int)i;
  scl_concurrent_segtree_t t;
  scl_csegtree_init(alloc, &t, N, sizeof(int), data, combine_min);

  atomic_init(&csegtree_updates, 0);

  pthread_t threads[NTHREADS];
  for (int i = 0; i < NTHREADS; i++)
    pthread_create(&threads[i], NULL, csegtree_update_thread, &t);
  for (int i = 0; i < NTHREADS; i++)
    pthread_join(threads[i], NULL);

  SCL_EXPECT_EQ_I(tr, atomic_load(&csegtree_updates), NTHREADS * N);

  int result;
  scl_csegtree_query(&t, 0, N - 1, &result);
  SCL_EXPECT_EQ_I(tr, result, -1);

  scl_csegtree_destroy(alloc, &t);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_csegtree_init_destroy(&tr);
  test_csegtree_query(&tr);
  test_csegtree_update(&tr);
  test_csegtree_concurrent_update(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
